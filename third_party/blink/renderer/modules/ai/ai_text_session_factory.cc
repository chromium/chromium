// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_text_session_factory.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom-blink.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ai/ai_text_session.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

AITextSessionFactory::AITextSessionFactory(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      ai_remote_(context),
      task_runner_(task_runner) {
  CHECK(context);
  context->GetBrowserInterfaceBroker().GetInterface(
      ai_remote_.BindNewPipeAndPassReceiver(task_runner_));
}

void AITextSessionFactory::Trace(Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_remote_);
}

HeapMojoRemote<mojom::blink::AIManager>& AITextSessionFactory::GetAIRemote() {
  if (!ai_remote_.is_bound()) {
    if (GetExecutionContext()) {
      GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
          ai_remote_.BindNewPipeAndPassReceiver(task_runner_));
    }
  }
  return ai_remote_;
}

void AITextSessionFactory::CanCreateTextSession(
    AIMetrics::AISessionType session_type,
    CanCreateTextSessionCallback callback) {
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(session_type),
      AIMetrics::AIAPI::kCanCreateSession);
  if (!GetAIRemote().is_connected()) {
    std::move(callback).Run(
        AICapabilityAvailability::kNo,
        mojom::blink::ModelAvailabilityCheckResult::kNoServiceNotRunning);
    return;
  }

  GetAIRemote()->CanCreateTextSession(WTF::BindOnce(
      [](AITextSessionFactory* factory, AIMetrics::AISessionType session_type,
         CanCreateTextSessionCallback callback,
         mojom::blink::ModelAvailabilityCheckResult result) {
        AICapabilityAvailability availability =
            HandleModelAvailabilityCheckResult(factory->GetExecutionContext(),
                                               session_type, result);
        std::move(callback).Run(availability, result);
      },
      WrapWeakPersistent(this), session_type, std::move(callback)));
}

void AITextSessionFactory::CreateTextSession(
    AIMetrics::AISessionType session_type,
    mojom::blink::AITextSessionSamplingParamsPtr sampling_params,
    const WTF::String& system_prompt,
    Vector<mojom::blink::AIAssistantInitialPromptPtr> initial_prompts,
    CreateTextSessionCallback callback) {
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(session_type),
      AIMetrics::AIAPI::kCreateSession);
  if (!GetAIRemote().is_connected()) {
    std::move(callback).Run(
        base::unexpected<DOMException*>(CreateInternalErrorException()));
    return;
  }
  AITextSession* text_session =
      MakeGarbageCollected<AITextSession>(GetExecutionContext(), task_runner_);
  GetAIRemote()->CreateTextSession(
      text_session->GetModelSessionReceiver(), std::move(sampling_params),
      system_prompt, std::move(initial_prompts),
      WTF::BindOnce(
          [](CreateTextSessionCallback callback, AITextSession* text_session,
             blink::mojom::blink::AITextSessionInfoPtr info) {
            if (info) {
              text_session->SetInfo(base::PassKey<AITextSessionFactory>(),
                                    std::move(info));
              std::move(callback).Run(text_session);
            } else {
              std::move(callback).Run(
                  base::unexpected<DOMException*>(DOMException::Create(
                      kExceptionMessageUnableToCreateSession,
                      DOMException::GetErrorName(
                          DOMExceptionCode::kInvalidStateError))));
            }
          },
          std::move(callback), WrapPersistent(text_session)));
}

}  // namespace blink
