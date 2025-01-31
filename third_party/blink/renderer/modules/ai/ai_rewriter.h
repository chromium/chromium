// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_REWRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_REWRITER_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_rewriter_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {

class AIRewriterRewriteOptions;
class ReadableStream;

// The class that represents a rewriter object.
class AIRewriter final : public ScriptWrappable, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AIRewriter(ExecutionContext* execution_context,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             mojo::PendingRemote<mojom::blink::AIRewriter> pending_remote,
             AIRewriterCreateOptions* options);
  void Trace(Visitor* visitor) const override;

  // ai_rewriter.idl implementation.
  ScriptPromise<IDLString> rewrite(ScriptState* script_state,
                                   const String& input,
                                   const AIRewriterRewriteOptions* options,
                                   ExceptionState& exception_state);
  ReadableStream* rewriteStreaming(ScriptState* script_state,
                                   const String& input,
                                   const AIRewriterRewriteOptions* options,
                                   ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);
  String sharedContext() const {
    return options_->getSharedContextOr(g_empty_string);
  }
  V8AIRewriterTone tone() const { return options_->tone(); }
  V8AIRewriterFormat format() const { return options_->format(); }
  V8AIRewriterLength length() const { return options_->length(); }
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
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::AIRewriter> remote_;
  Member<AIRewriterCreateOptions> options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_REWRITER_H_
