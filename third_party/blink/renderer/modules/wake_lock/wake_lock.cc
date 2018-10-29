// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

#include "services/device/public/mojom/constants.mojom-blink.h"
#include "services/device/public/mojom/wake_lock_provider.mojom-blink.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_request.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

WakeLock* WakeLock::CreateScreenWakeLock(ScriptState* script_state) {
  return new WakeLock(script_state, LockType::kScreen);
}

WakeLock* WakeLock::CreateSystemWakeLock(ScriptState* script_state) {
  return new WakeLock(script_state, LockType::kSystem);
}

WakeLock::~WakeLock() = default;

WakeLock::WakeLock(ScriptState* script_state, LockType type)
    : ContextLifecycleObserver(blink::ExecutionContext::From(script_state)),
      PageVisibilityObserver(
          To<Document>(blink::ExecutionContext::From(script_state))->GetPage()),
      type_(type) {
}

ScriptPromise WakeLock::GetPromise(ScriptState* script_state) {
  if (!wake_lock_property_) {
    wake_lock_property_ = new WakeLockProperty(
        ExecutionContext::From(script_state), this, WakeLockProperty::kReady);
    wake_lock_property_->Resolve(this);
  }
  return wake_lock_property_->Promise(script_state->World());
}

AtomicString WakeLock::type() const {
  switch (type_) {
    case LockType::kSystem:
      return "system";
    case LockType::kScreen:
      return "screen";
  }

  NOTREACHED();
  return AtomicString();
}

bool WakeLock::active() const {
  return active_;
}

void WakeLock::OnConnectionError() {
  wake_lock_service_.reset();
}

void WakeLock::ChangeActiveStatus(bool active) {
  if (active_ == active)
    return;

  BindToServiceIfNeeded();
  if (active)
    wake_lock_service_->RequestWakeLock();
  else
    wake_lock_service_->CancelWakeLock();

  active_ = active;
  EnqueueEvent(*Event::Create(EventTypeNames::activechange),
               TaskType::kMiscPlatformAPI);
}

void WakeLock::BindToServiceIfNeeded() {
  if (wake_lock_service_)
    return;

  device::mojom::blink::WakeLockType type;
  switch (type_) {
    case LockType::kSystem:
      type = device::mojom::blink::WakeLockType::kPreventAppSuspension;
      break;
    case LockType::kScreen:
      type = device::mojom::blink::WakeLockType::kPreventDisplaySleep;
      break;
  }

  device::mojom::blink::WakeLockProviderPtr provider;
  Platform::Current()->GetConnector()->BindInterface(
      device::mojom::blink::kServiceName, mojo::MakeRequest(&provider));
  provider->GetWakeLockWithoutContext(
      type, device::mojom::blink::WakeLockReason::kOther, "Blink Wake Lock",
      mojo::MakeRequest(&wake_lock_service_));

  wake_lock_service_.set_connection_error_handler(
      WTF::Bind(&WakeLock::OnConnectionError, WrapWeakPersistent(this)));
}

WakeLockRequest* WakeLock::createRequest() {
  if (!active_ && request_counter_ == 0)
    ChangeActiveStatus(true);

  request_counter_++;
  return new WakeLockRequest(this);
}

void WakeLock::CancelRequest() {
  DCHECK_GT(request_counter_, 0);
  if (active_ && request_counter_ == 1)
    ChangeActiveStatus(false);

  request_counter_--;
}

const WTF::AtomicString& WakeLock::InterfaceName() const {
  return EventTargetNames::WakeLock;
}

ExecutionContext* WakeLock::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool WakeLock::HasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object if there are
  // event listeners attached to it.
  return GetExecutionContext() && HasEventListeners();
}

void WakeLock::ContextDestroyed(ExecutionContext*) {
  ChangeActiveStatus(false);
}

void WakeLock::PageVisibilityChanged() {
  ChangeActiveStatus(GetPage() && GetPage()->IsPageVisible());
}

void WakeLock::Trace(blink::Visitor* visitor) {
  visitor->Trace(wake_lock_property_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

}  // namespace blink
