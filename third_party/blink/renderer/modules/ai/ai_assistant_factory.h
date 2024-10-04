// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_FACTORY_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_assistant_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant_capabilities.h"

namespace blink {

class AI;
class AIAssistant;

// This class is responsible for creating AIAssistant instances.
class AIAssistantFactory final : public ScriptWrappable,
                                 public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AIAssistantFactory(AI* ai);
  void Trace(Visitor* visitor) const override;

  // ai_assistant_factory.idl implementation.
  ScriptPromise<AIAssistant> create(ScriptState* script_state,
                                    const AIAssistantCreateOptions* options,
                                    ExceptionState& exception_state);
  ScriptPromise<AIAssistantCapabilities> capabilities(
      ScriptState* script_state,
      ExceptionState& exception_state);

  ~AIAssistantFactory() override = default;

 private:
  void OnGetModelInfoComplete(
      ScriptPromiseResolver<AIAssistantCapabilities>* resolver,
      AIAssistantCapabilities* capabilities,
      mojom::blink::AIModelInfoPtr model_info);
  void OnCanCreateSessionComplete(
      ScriptPromiseResolver<AIAssistantCapabilities>* resolver,
      mojom::blink::ModelAvailabilityCheckResult check_result);

  Member<AI> ai_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_FACTORY_H_
