// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_text_session.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// The class that represents a summarizer object.
class AISummarizer final : public ScriptWrappable,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AISummarizer(ExecutionContext* context,
               AITextSession* text_session,
               scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AISummarizer() override = default;

  void Trace(Visitor* visitor) const override;

  // ai_summarizer.idl implementation.
  ScriptPromise<IDLString> summarize(ScriptState* script_state,
                                     const WTF::String& input,
                                     ExceptionState& exception_state);
  ReadableStream* summarizeStreaming(ScriptState* script_state,
                                     const WTF::String& input,
                                     ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

 private:
  Member<AITextSession> text_session_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_H_
