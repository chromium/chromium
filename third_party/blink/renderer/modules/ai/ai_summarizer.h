// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_

#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_length.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_summarize_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// The class that represents a summarizer object.
class AISummarizer final : public ScriptWrappable,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AISummarizer(ExecutionContext* context,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               mojo::PendingRemote<mojom::blink::AISummarizer> pending_remote,
               const WTF::String& shared_context,
               V8AISummarizerType type,
               V8AISummarizerFormat format,
               V8AISummarizerLength length);
  ~AISummarizer() override = default;

  void Trace(Visitor* visitor) const override;

  // ai_summarizer.idl implementation.
  ScriptPromise<IDLString> summarize(
      ScriptState* script_state,
      const WTF::String& input,
      const AISummarizerSummarizeOptions* options,
      ExceptionState& exception_state);
  ReadableStream* summarizeStreaming(
      ScriptState* script_state,
      const WTF::String& input,
      const AISummarizerSummarizeOptions* options,
      ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);
  WTF::String sharedContext() { return shared_context_; }
  V8AISummarizerType type() { return type_; }
  V8AISummarizerFormat format() { return format_; }
  V8AISummarizerLength length() { return length_; }

 private:
  bool is_destroyed_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<blink::mojom::blink::AISummarizer> summarizer_remote_;

  WTF::String shared_context_;
  V8AISummarizerType type_;
  V8AISummarizerFormat format_;
  V8AISummarizerLength length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_
