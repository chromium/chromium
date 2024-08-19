// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_text_session_factory.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// The class that represents a session with simple generic model execution.
class AITextSession final : public ScriptWrappable,
                            public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AITextSession(ExecutionContext* context,
                scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AITextSession() override = default;

  void Trace(Visitor* visitor) const override;

  mojo::PendingReceiver<blink::mojom::blink::AITextSession>
  GetModelSessionReceiver();

  HeapMojoRemote<blink::mojom::blink::AITextSession>& GetRemoteTextSession() {
    return text_session_remote_;
  }

  void SetInfo(base::PassKey<AITextSessionFactory>,
               blink::mojom::blink::AITextSessionInfoPtr info);

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
  uint64_t maxTokens() const;
  uint64_t tokensSoFar() const;
  uint64_t tokensLeft() const;

  uint32_t topK() const;
  float temperature() const;

  ScriptPromise<AITextSession> clone(ScriptState* script_state,
                                     ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

 private:
  // Checks and returns if the session is already destroyed. If the session is
  // destroyed, throw an exception with the corresponding error.
  bool ThrowExceptionIfIsDestroyed(ExceptionState& exception_state);
  void OnResponseComplete(std::optional<uint64_t> current_tokens);

  blink::mojom::blink::AITextSessionInfoPtr info_;
  uint64_t current_tokens_ = 0;

  bool is_destroyed_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<blink::mojom::blink::AITextSession> text_session_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_TEXT_SESSION_H_
