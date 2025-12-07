// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_CREATE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_CREATE_CLIENT_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/create_monitor.h"
#include "third_party/blink/renderer/modules/ai/language_model.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

// TODO(crbug.com/416021087): Consolidate with AIWritingAssistanceCreateClient.
class LanguageModelCreateClient
    : public GarbageCollected<LanguageModelCreateClient>,
      public mojom::blink::AIManagerCreateLanguageModelClient,
      public ExecutionContextClient,
      public AIContextObserver<LanguageModel> {
 public:
  LanguageModelCreateClient(
      ScriptPromiseResolver<LanguageModel>* resolver,
      LanguageModelCreateOptions* options,
      mojom::blink::AILanguageModelSamplingParamsPtr resolved_sampling_params);
  ~LanguageModelCreateClient() override;

  LanguageModelCreateClient(const LanguageModelCreateClient&) = delete;
  LanguageModelCreateClient& operator=(const LanguageModelCreateClient&) =
      delete;

  void Trace(Visitor* visitor) const override;

  // mojom::blink::AIManagerCreateLanguageModelClient:
  void OnResult(
      mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote,
      mojom::blink::AILanguageModelInstanceInfoPtr info) override;
  void OnError(mojom::blink::AIManagerCreateClientError error,
               mojom::blink::QuotaErrorInfoPtr quota_error_info) override;

  // AIContextObserver:
  void ResetReceiver() override;

 private:
  // Process options and create, if the availability result is valid.
  void Create(mojom::blink::ModelAvailabilityCheckResult result);

  // Continue creation after any initial prompts were processed or rejected.
  void OnInitialPromptsResolved(
      Vector<mojom::blink::AILanguageModelExpectedPtr> expected_inputs,
      Vector<mojom::blink::AILanguageModelExpectedPtr> expected_outputs,
      Vector<mojom::blink::AILanguageModelPromptPtr> initial_prompts);
  void OnInitialPromptsRejected(const ScriptValue& error);

  HeapMojoReceiver<mojom::blink::AIManagerCreateLanguageModelClient,
                   LanguageModelCreateClient>
      receiver_;
  Member<LanguageModelCreateOptions> options_;
  Member<CreateMonitor> monitor_;
  mojom::blink::AILanguageModelSamplingParamsPtr resolved_sampling_params_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_LANGUAGE_MODEL_CREATE_CLIENT_H_
