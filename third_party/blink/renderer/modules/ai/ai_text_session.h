// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// The class that represents a session with simple generic model execution.
class AITextSession final : public ScriptWrappable,
                            public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Implementation of blink::mojom::blink::ModelStreamingResponder that
  // handles the streaming output of the model execution, and returns the full
  // result through a promise.
  class Responder final : public GarbageCollected<Responder>,
                          public blink::mojom::blink::ModelStreamingResponder,
                          public ContextLifecycleObserver {
   public:
    explicit Responder(blink::ScriptState* script_state);
    ~Responder() override;

    void Trace(Visitor* visitor) const override;

    ScriptPromiseResolver<IDLString>* GetResolver();

    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
    BindNewPipeAndPassRemote(
        scoped_refptr<base::SequencedTaskRunner> task_runner);

    // `blink::mojom::blink::ModelStreamingResponder` implementation.
    void OnResponse(blink::mojom::blink::ModelStreamingResponseStatus status,
                    const WTF::String& text) override;

    // ContextLifecycleObserver implementation.
    void ContextDestroyed() override { Cleanup(); }

   private:
    void Cleanup();

    Member<ScriptPromiseResolver<IDLString>> resolver_;
    WTF::String response_;
    int response_callback_count_;

    HeapMojoReceiver<blink::mojom::blink::ModelStreamingResponder, Responder>
        receiver_;
    SelfKeepAlive<Responder> keep_alive_{this};
  };

  // Implementation of blink::mojom::blink::ModelStreamingResponder that
  // handles the streaming output of the model execution, and returns the full
  // result through a ReadableStream.
  class StreamingResponder final
      : public UnderlyingSourceBase,
        public blink::mojom::blink::ModelStreamingResponder {
   public:
    explicit StreamingResponder(blink::ScriptState* script_state);
    ~StreamingResponder() override;

    void Trace(Visitor* visitor) const override;

    mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
    BindNewPipeAndPassRemote(
        scoped_refptr<base::SequencedTaskRunner> task_runner);

    // `UnderlyingSourceBase` implementation.
    ScriptPromiseUntyped Pull(ScriptState* script_state,
                              ExceptionState& exception_state) override;

    ScriptPromiseUntyped Cancel(ScriptState* script_state,
                                ScriptValue reason,
                                ExceptionState& exception_state) override;

    // `blink::mojom::blink::ModelStreamingResponder` implementation.
    void OnResponse(blink::mojom::blink::ModelStreamingResponseStatus status,
                    const WTF::String& text) override;

   private:
    int response_size_;
    int response_callback_count_;
    Member<ScriptState> script_state_;
    HeapMojoReceiver<blink::mojom::blink::ModelStreamingResponder,
                     StreamingResponder>
        receiver_;
  };

  AITextSession(ExecutionContext* context,
                scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AITextSession() override = default;

  void Trace(Visitor* visitor) const override;

  mojo::PendingReceiver<blink::mojom::blink::AITextSession>
  GetModelSessionReceiver();

  HeapMojoRemote<blink::mojom::blink::AITextSession>& GetRemoteTextSession() {
    return text_session_remote_;
  }

  // ai_text_session.idl implementation.
  // TODO(crbug.com/356302845): The prompt APIs will be moved to the AIAssistant
  // class and the AITextSession class will be a lightweight wrapper for
  // text_session_remote_.
  ScriptPromise<IDLString> prompt(ScriptState* script_state,
                                  const WTF::String& input,
                                  ExceptionState& exception_state);
  ReadableStream* promptStreaming(ScriptState* script_state,
                                  const WTF::String& input,
                                  ExceptionState& exception_state);
  ScriptPromise<AITextSession> clone(ScriptState* script_state,
                                     ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

 private:
  // Checks and returns if the session is already destroyed. If the session is
  // destroyed, throw an exception with the corresponding error.
  bool ThrowExceptionIfIsDestroyed(ExceptionState& exception_state);

  bool is_destroyed_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<blink::mojom::blink::AITextSession> text_session_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_
