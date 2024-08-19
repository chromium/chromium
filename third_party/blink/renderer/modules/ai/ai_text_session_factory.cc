// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_text_session_factory.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom-blink.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
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
    CanCreateTextSessionCallback callback) {
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kCanCreateSession);
  if (!GetAIRemote().is_connected()) {
    std::move(callback).Run(
        AICapabilityAvailability::kNo,
        mojom::blink::ModelAvailabilityCheckResult::kNoServiceNotRunning);
    return;
  }

  GetAIRemote()->CanCreateTextSession(WTF::BindOnce(
      [](AITextSessionFactory* factory, CanCreateTextSessionCallback callback,
         mojom::blink::ModelAvailabilityCheckResult result) {
        AICapabilityAvailability availability;
        if (result == mojom::blink::ModelAvailabilityCheckResult::kReadily) {
          availability = AICapabilityAvailability::kReadily;
        } else if (result ==
                   mojom::blink::ModelAvailabilityCheckResult::kAfterDownload) {
          // TODO(crbug.com/345357441): Implement the
          // `ontextmodeldownloadprogress` event.
          availability = AICapabilityAvailability::kAfterDownload;
        } else {
          // If the text session cannot be created, logs the error message to
          // the console.
          availability = AICapabilityAvailability::kNo;
          factory->GetExecutionContext()->AddConsoleMessage(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              ConvertModelAvailabilityCheckResultToDebugString(result));
        }
        base::UmaHistogramEnumeration(
            AIMetrics::GetAICapabilityAvailabilityMetricName(
                AIMetrics::AISessionType::kText),
            availability);
        std::move(callback).Run(availability, result);
      },
      WrapWeakPersistent(this), std::move(callback)));
}

void AITextSessionFactory::CreateTextSession(
    mojom::blink::AITextSessionSamplingParamsPtr sampling_params,
    const WTF::String& system_prompt,
    CreateTextSessionCallback callback) {
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
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
      system_prompt,
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
