// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_

#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_create_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_length.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_summarize_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// The class that represents a summarizer object.
class AISummarizer final : public ScriptWrappable,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AISummarizer(ExecutionContext* context,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               mojo::PendingRemote<mojom::blink::AISummarizer> pending_remote,
               AISummarizerCreateOptions* options);
  ~AISummarizer() override = default;

  void Trace(Visitor* visitor) const override;

  // ai_summarizer.idl implementation.
  ScriptPromise<IDLString> summarize(
      ScriptState* script_state,
      const String& input,
      const AISummarizerSummarizeOptions* options,
      ExceptionState& exception_state);
  ReadableStream* summarizeStreaming(
      ScriptState* script_state,
      const String& input,
      const AISummarizerSummarizeOptions* options,
      ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);
  String sharedContext() const {
    return options_->getSharedContextOr(g_empty_string);
  }
  V8AISummarizerType type() const { return options_->type(); }
  V8AISummarizerFormat format() const { return options_->format(); }
  V8AISummarizerLength length() const { return options_->length(); }
  std::optional<Vector<String>> expectedInputLanguages() const {
    if (options_->hasExpectedInputLanguages()) {
      return options_->expectedInputLanguages();
    }
    return std::nullopt;
  }
  std::optional<Vector<String>> expectedContextLanguages() const {
    if (options_->hasExpectedContextLanguages()) {
      return options_->expectedContextLanguages();
    }
    return std::nullopt;
  }
  String outputLanguage() const {
    return options_->getOutputLanguageOr(String());
  }

 private:
  bool is_destroyed_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<blink::mojom::blink::AISummarizer> summarizer_remote_;
  Member<AISummarizerCreateOptions> options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_
