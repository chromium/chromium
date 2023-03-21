// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_resource_manager.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_reader_presence_event.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_reader_presence_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
const char kWatchForReadersNotSupported[] =
    "Watching for reader addition/removal is not supported in this platform.";
const char kContextGone[] = "Script context has shut down.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"smart-card\" is disallowed by permissions policy.";

bool ShouldBlockSmartCardServiceCall(ExecutionContext* context,
                                     ExceptionState& exception_state) {
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kContextGone);
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

void SmartCardResourceManager::ReaderAdded(SmartCardReaderInfoPtr reader_info) {
  // Create the reader even if there's no presence_observer_ in order
  // to add it to the cache.
  SmartCardReader* reader = GetOrCreateReader(std::move(reader_info));

  if (presence_observer_) {
    presence_observer_->DispatchEvent(
        *MakeGarbageCollected<SmartCardReaderPresenceEvent>(
            event_type_names::kReaderadd, reader));
  }
}

void SmartCardResourceManager::ReaderRemoved(
    SmartCardReaderInfoPtr reader_info) {
  if (presence_observer_) {
    SmartCardReader* reader = GetOrCreateReader(std::move(reader_info));
    presence_observer_->DispatchEvent(
        *MakeGarbageCollected<SmartCardReaderPresenceEvent>(
            event_type_names::kReaderremove, reader));
    reader_cache_.erase(reader->name());
  } else {
    reader_cache_.erase(reader_info->name);
  }
}

void SmartCardResourceManager::ReaderChanged(
    SmartCardReaderInfoPtr reader_info) {
  auto it = reader_cache_.find(reader_info->name);
  if (it != reader_cache_.end()) {
    it->value->UpdateInfo(std::move(reader_info));
    return;
  }

  // If the name is not in the |reader_cache_| then this is the first time we
  // have been notified for this reader.
  ReaderAdded(std::move(reader_info));
}

void SmartCardResourceManager::Error(SmartCardResponseCode response_code) {
  tracking_started_ = false;
  // TODO(crbug.com/1386175):
  // * Put existing SmartCardReader instances into an invalid state.
  // * Forward error to existing SmartCardPresenceObservers.
}

void SmartCardResourceManager::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(get_readers_promises_);
  visitor->Trace(watch_for_readers_promises_);
  visitor->Trace(reader_cache_);
  visitor->Trace(presence_observer_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

ScriptPromise SmartCardResourceManager::getReaders(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (ShouldBlockSmartCardServiceCall(GetExecutionContext(), exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  get_readers_promises_.insert(resolver);

  EnsureServiceConnection();

  tracking_started_ = true;
  service_->GetReadersAndStartTracking(
      WTF::BindOnce(&SmartCardResourceManager::FinishGetReaders,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise SmartCardResourceManager::watchForReaders(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (ShouldBlockSmartCardServiceCall(GetExecutionContext(), exception_state)) {
    return ScriptPromise();
  }

  EnsureServiceConnection();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  // Get the promise before it can be resolved by ResolveWatchForReadersPromise
  ScriptPromise promise = resolver->Promise();

  if (supports_reader_presence_observer_.has_value()) {
    ResolveWatchForReadersPromise(resolver);
  } else {
    // Wait until supports_reader_presence_observer_ has a value
    watch_for_readers_promises_.insert(resolver);
  }

  return promise;
}

void SmartCardResourceManager::FinishGetReaders(
    ScriptPromiseResolver* resolver,
    mojom::blink::SmartCardGetReadersResultPtr result) {
  DCHECK(get_readers_promises_.Contains(resolver));
  get_readers_promises_.erase(resolver);

  if (result->is_response_code()) {
    auto* error = SmartCardError::Create(result->get_response_code());
    resolver->Reject(error);
    return;
  }

  HeapVector<Member<SmartCardReader>> readers;
  for (auto& reader_info : result->get_readers()) {
    readers.push_back(GetOrCreateReader(std::move(reader_info)));
  }

  resolver->Resolve(readers);
}

void SmartCardResourceManager::UpdateReadersCache(
    mojom::blink::SmartCardGetReadersResultPtr result) {
  if (result->is_response_code()) {
    return;
  }

  for (auto& reader_info : result->get_readers()) {
    GetOrCreateReader(std::move(reader_info));
  }
}

SmartCardReader* SmartCardResourceManager::GetOrCreateReader(
    mojom::blink::SmartCardReaderInfoPtr info) {
  auto it = reader_cache_.find(info->name);
  if (it != reader_cache_.end()) {
    return it->value;
  }

  const String name = info->name;
  SmartCardReader* reader = MakeGarbageCollected<SmartCardReader>(
      std::move(info), GetExecutionContext());

  reader_cache_.insert(name, reader);
  return reader;
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
  DCHECK(!receiver_.is_bound());
  service_->RegisterClient(
      receiver_.BindNewEndpointAndPassRemote(),
      WTF::BindOnce(&SmartCardResourceManager::OnServiceClientRegistered,
                    WrapWeakPersistent(this)));
}

void SmartCardResourceManager::OnServiceClientRegistered(
    bool supports_reader_presence_observer) {
  if (supports_reader_presence_observer_.has_value()) {
    // We already got it from a previous RegisterClient() call and its value
    // is not expected to change.
    DCHECK_EQ(*supports_reader_presence_observer_,
              supports_reader_presence_observer);
    // There should be no pending promises for watchForReaders() as it depends
    // only on knowledge of this boolean value.
    DCHECK(watch_for_readers_promises_.empty());
    return;
  }

  supports_reader_presence_observer_ = supports_reader_presence_observer;

  // Resolving a promise can invoke a script which calls watchForReaders().
  // But if it happens that won't affect watch_for_readers_promises_ since
  // promises are only added there when supports_reader_presence_observer_
  // does not have a value, which is not the case anymore.
  for (auto& resolver : watch_for_readers_promises_) {
    ResolveWatchForReadersPromise(resolver.Get());
  }
  watch_for_readers_promises_.clear();
}

SmartCardReaderPresenceObserver*
SmartCardResourceManager::GetOrCreatePresenceObserver() {
  if (!presence_observer_) {
    presence_observer_ = MakeGarbageCollected<SmartCardReaderPresenceObserver>(
        GetExecutionContext());
  }

  return presence_observer_;
}

void SmartCardResourceManager::ResolveWatchForReadersPromise(
    ScriptPromiseResolver* resolver) {
  DCHECK(supports_reader_presence_observer_.has_value());

  if (!supports_reader_presence_observer_.value()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                     kWatchForReadersNotSupported);
    return;
  }

  if (!tracking_started_) {
    tracking_started_ = true;
    service_->GetReadersAndStartTracking(WTF::BindOnce(
        &SmartCardResourceManager::UpdateReadersCache, WrapPersistent(this)));
  }

  // TODO(crbug.com/1386175): possibly always create a new observer.
  resolver->Resolve(GetOrCreatePresenceObserver());
}

void SmartCardResourceManager::CloseServiceConnection() {
  service_.reset();
  // TODO(crbug.com/1386175): Deal with promises in
  // get_readers_promises_ and watch_for_readers_promises_
}

}  // namespace blink
