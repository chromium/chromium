// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_FACTORY_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_summarizer_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_summarizer_capabilities.h"

namespace blink {

class AI;

class AISummarizer;

class AISummarizerFactory final : public ScriptWrappable,
                                  public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AISummarizerFactory(AI* ai,
                      ExecutionContext* context,
                      scoped_refptr<base::SequencedTaskRunner> task_runner);

  void Trace(Visitor* visitor) const override;

  // ai_summarizer_factory.idl implementation.
  ScriptPromise<AISummarizer> create(ScriptState* script_state,
                                     AISummarizerCreateOptions* options,
                                     ExceptionState& exception_state);
  ScriptPromise<AISummarizerCapabilities> capabilities(
      ScriptState* script_state,
      ExceptionState& exception_state);

  ~AISummarizerFactory() override = default;

 private:
  Member<AI> ai_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_SUMMARIZER_FACTORY_H_
