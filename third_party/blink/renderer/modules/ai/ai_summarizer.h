// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_summarize_options.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// The class that represents a summarizer object.
class AISummarizer final : public ScriptWrappable,
                           public AIWritingAssistanceBase<
                               AISummarizer,
                               mojom::blink::AISummarizer,
                               mojom::blink::AIManagerCreateSummarizerClient,
                               AISummarizerCreateCoreOptions,
                               AISummarizerCreateOptions,
                               AISummarizerSummarizeOptions> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AISummarizer(ExecutionContext* execution_context,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               mojo::PendingRemote<mojom::blink::AISummarizer> pending_remote,
               AISummarizerCreateOptions* options);
  void Trace(Visitor* visitor) const override;

  // AIWritingAssistanceBase:
  void remoteExecute(
      const String& input,
      const String& context,
      mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
          responder) override;

  // ai_summarizer.idl:
  ScriptPromise<IDLString> summarize(
      ScriptState* script_state,
      const String& writing_task,
      const AISummarizerSummarizeOptions* options,
      ExceptionState& exception_state);
  ReadableStream* summarizeStreaming(
      ScriptState* script_state,
      const String& writing_task,
      const AISummarizerSummarizeOptions* options,
      ExceptionState& exception_state);
  ScriptPromise<IDLDouble> measureInputUsage(
      ScriptState* script_state,
      const String& input,
      const AISummarizerSummarizeOptions* options,
      ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

  V8AISummarizerType type() const { return options_->type(); }
  V8AISummarizerFormat format() const { return options_->format(); }
  V8AISummarizerLength length() const { return options_->length(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_
