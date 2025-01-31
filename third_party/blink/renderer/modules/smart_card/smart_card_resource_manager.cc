// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_resource_manager.h"

#include "services/device/public/mojom/smart_card.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_context.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
constexpr char kContextGone[] = "Script context has shut down.";
constexpr char kFeaturePolicyBlocked[] =
    "Access to the feature \"smart-card\" is disallowed by permissions policy.";
constexpr char kNotSufficientlyIsolated[] =
    "Frame is not sufficiently isolated to use smart cards.";
constexpr char kServiceDisconnected[] =
    "Disconnected from the smart card service.";

bool ShouldBlockSmartCardServiceCall(ExecutionContext* context,
                                     ExceptionState& exception_state) {
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
  } else if (!context->IsIsolatedContext() ||
             !context->IsFeatureEnabled(mojom::blink::PermissionsPolicyFeature::
                                            kCrossOriginIsolated)) {
    exception_state.ThrowSecurityError(kNotSufficientlyIsolated);
  } else if (!context->IsFeatureEnabled(
                 mojom::blink::PermissionsPolicyFeature::kSmartCard,
                 ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
  }

  return exception_state.HadException();
}

}  // namespace

const char SmartCardResourceManager::kSupplementName[] =
    "SmartCardResourceManager";

SmartCardResourceManager* SmartCardResourceManager::smartCard(
    NavigatorBase& navigator) {
  SmartCardResourceManager* smartcard =
      Supplement<NavigatorBase>::From<SmartCardResourceManager>(navigator);
  if (!smartcard) {
    smartcard = MakeGarbageCollected<SmartCardResourceManager>(navigator);
    ProvideTo(navigator, smartcard);
  }
  return smartcard;
}

SmartCardResourceManager::SmartCardResourceManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      service_(navigator.GetExecutionContext()) {}

void SmartCardResourceManager::ContextDestroyed() {
  CloseServiceConnection();
}

void SmartCardResourceManager::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(create_context_promises_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

ScriptPromise<SmartCardContext> SmartCardResourceManager::establishContext(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (ShouldBlockSmartCardServiceCall(GetExecutionContext(), exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<SmartCardContext>>(
          script_state, exception_state.GetContext());
  create_context_promises_.insert(resolver);

  EnsureServiceConnection();

  service_->CreateContext(
      WTF::BindOnce(&SmartCardResourceManager::OnCreateContextDone,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

void SmartCardResourceManager::EnsureServiceConnection() {
  DCHECK(GetExecutionContext());

  if (service_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(
      WTF::BindOnce(&SmartCardResourceManager::CloseServiceConnection,
                    WrapWeakPersistent(this)));
}

void SmartCardResourceManager::OnCreateContextDone(
    ScriptPromiseResolver<SmartCardContext>* resolver,
    device::mojom::blink::SmartCardCreateContextResultPtr result) {
  DCHECK(create_context_promises_.Contains(resolver));
  create_context_promises_.erase(resolver);

  if (result->is_error()) {
    SmartCardError::MaybeReject(resolver, result->get_error());
    return;
  }

  auto* context = MakeGarbageCollected<SmartCardContext>(
      std::move(result->get_context()), GetExecutionContext());

  resolver->Resolve(context);
}

void SmartCardResourceManager::CloseServiceConnection() {
  service_.reset();

  for (auto& resolver : create_context_promises_) {
    ScriptState* script_state = resolver->GetScriptState();
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       script_state)) {
      continue;
    }
    ScriptState::Scope script_state_scope(script_state);
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kServiceDisconnected);
  }
  create_context_promises_.clear();
}

}  // namespace blink
