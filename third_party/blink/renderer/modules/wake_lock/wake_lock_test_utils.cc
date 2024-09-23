// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"

#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_wake_lock_sentinel.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::PermissionDescriptorPtr;

namespace {

void RunWithStack(base::RunLoop* run_loop) {
  run_loop->Run();
}

// Helper class for WaitForPromise{Fulfillment,Rejection}(). It provides a
// function that invokes |callback| when a ScriptPromiseUntyped is resolved.
class ClosureRunnerCallable final : public ScriptFunction::Callable {
 public:
  explicit ClosureRunnerCallable(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  ScriptValue Call(ScriptState*, ScriptValue) override {
    if (callback_)
      std::move(callback_).Run();
    return ScriptValue();
  }

 private:
  base::OnceClosure callback_;
};

V8WakeLockType::Enum ToBlinkWakeLockType(
    device::mojom::blink::WakeLockType type) {
  switch (type) {
    case device::mojom::blink::WakeLockType::kPreventDisplaySleep:
    case device::mojom::blink::WakeLockType::kPreventDisplaySleepAllowDimming:
      return V8WakeLockType::Enum::kScreen;
    case device::mojom::blink::WakeLockType::kPreventAppSuspension:
      return V8WakeLockType::Enum::kSystem;
  }
}

}  // namespace

// MockWakeLock

MockWakeLock::MockWakeLock() = default;
MockWakeLock::~MockWakeLock() = default;

void MockWakeLock::Bind(
    mojo::PendingReceiver<device::mojom::blink::WakeLock> receiver) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&MockWakeLock::OnConnectionError, WTF::Unretained(this)));
}

void MockWakeLock::Unbind() {
  OnConnectionError();
}

void MockWakeLock::WaitForRequest() {
  DCHECK(!request_wake_lock_callback_);
  base::RunLoop run_loop;
  request_wake_lock_callback_ = run_loop.QuitClosure();
  RunWithStack(&run_loop);
}

void MockWakeLock::WaitForCancelation() {
  DCHECK(!cancel_wake_lock_callback_);
  if (!receiver_.is_bound()) {
    // If OnConnectionError() has been called, bail out early to avoid waiting
    // forever.
    DCHECK(!is_acquired_);
    return;
  }
  base::RunLoop run_loop;
  cancel_wake_lock_callback_ = run_loop.QuitClosure();
  RunWithStack(&run_loop);
}

void MockWakeLock::OnConnectionError() {
  receiver_.reset();
  CancelWakeLock();
}

void MockWakeLock::RequestWakeLock() {
  is_acquired_ = true;
  if (request_wake_lock_callback_)
    std::move(request_wake_lock_callback_).Run();
}

void MockWakeLock::CancelWakeLock() {
  is_acquired_ = false;
  if (cancel_wake_lock_callback_)
    std::move(cancel_wake_lock_callback_).Run();
}

void MockWakeLock::AddClient(
    mojo::PendingReceiver<device::mojom::blink::WakeLock>) {}
void MockWakeLock::ChangeType(device::mojom::blink::WakeLockType,
                              ChangeTypeCallback) {}
void MockWakeLock::HasWakeLockForTests(HasWakeLockForTestsCallback) {}

// MockWakeLockService

MockWakeLockService::MockWakeLockService() = default;
MockWakeLockService::~MockWakeLockService() = default;

void MockWakeLockService::BindRequest(mojo::ScopedMessagePipeHandle handle) {
  receivers_.Add(this, mojo::PendingReceiver<mojom::blink::WakeLockService>(
                           std::move(handle)));
}

MockWakeLock& MockWakeLockService::get_wake_lock(V8WakeLockType::Enum type) {
  size_t pos = static_cast<size_t>(type);
  return mock_wake_lock_[pos];
}

void MockWakeLockService::GetWakeLock(
    device::mojom::blink::WakeLockType type,
    device::mojom::blink::WakeLockReason reason,
    const String& description,
    mojo::PendingReceiver<device::mojom::blink::WakeLock> receiver) {
  size_t pos = static_cast<size_t>(ToBlinkWakeLockType(type));
  mock_wake_lock_[pos].Bind(std::move(receiver));
}

// MockPermissionService

MockPermissionService::MockPermissionService() = default;
MockPermissionService::~MockPermissionService() = default;

void MockPermissionService::BindRequest(mojo::ScopedMessagePipeHandle handle) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(mojo::PendingReceiver<mojom::blink::PermissionService>(
      std::move(handle)));
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &MockPermissionService::OnConnectionError, WTF::Unretained(this)));
}

void MockPermissionService::SetPermissionResponse(
    V8WakeLockType::Enum type,
    mojom::blink::PermissionStatus status) {
  DCHECK(status == mojom::blink::PermissionStatus::GRANTED ||
         status == mojom::blink::PermissionStatus::DENIED);
  permission_responses_[static_cast<size_t>(type)] = status;
}

void MockPermissionService::OnConnectionError() {
  std::ignore = receiver_.Unbind();
}

bool MockPermissionService::GetWakeLockTypeFromDescriptor(
    const PermissionDescriptorPtr& descriptor,
    V8WakeLockType::Enum* output) {
  if (descriptor->name == mojom::blink::PermissionName::SCREEN_WAKE_LOCK) {
    *output = V8WakeLockType::Enum::kScreen;
    return true;
  }
  if (descriptor->name == mojom::blink::PermissionName::SYSTEM_WAKE_LOCK) {
    *output = V8WakeLockType::Enum::kSystem;
    return true;
  }
  return false;
}

void MockPermissionService::WaitForPermissionRequest(
    V8WakeLockType::Enum type) {
  size_t pos = static_cast<size_t>(type);
  DCHECK(!request_permission_callbacks_[pos]);
  base::RunLoop run_loop;
  request_permission_callbacks_[pos] = run_loop.QuitClosure();
  RunWithStack(&run_loop);
}

void MockPermissionService::HasPermission(PermissionDescriptorPtr permission,
                                          HasPermissionCallback callback) {
  V8WakeLockType::Enum type;
  if (!GetWakeLockTypeFromDescriptor(permission, &type)) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }
  size_t pos = static_cast<size_t>(type);
  DCHECK(permission_responses_[pos].has_value());
  std::move(callback).Run(permission_responses_[pos].value_or(
      mojom::blink::PermissionStatus::DENIED));
}

void MockPermissionService::RegisterPageEmbeddedPermissionControl(
    Vector<mojom::blink::PermissionDescriptorPtr> permissions,
    mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient> client) {
}

void MockPermissionService::RequestPageEmbeddedPermission(
    mojom::blink::EmbeddedPermissionRequestDescriptorPtr permissions,
    RequestPageEmbeddedPermissionCallback) {
  NOTREACHED_IN_MIGRATION();
}

void MockPermissionService::RequestPermission(
    PermissionDescriptorPtr permission,
    bool user_gesture,
    RequestPermissionCallback callback) {
  V8WakeLockType::Enum type;
  if (!GetWakeLockTypeFromDescriptor(permission, &type)) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }

  size_t pos = static_cast<size_t>(type);
  DCHECK(permission_responses_[pos].has_value());
  if (request_permission_callbacks_[pos])
    std::move(request_permission_callbacks_[pos]).Run();
  std::move(callback).Run(permission_responses_[pos].value_or(
      mojom::blink::PermissionStatus::DENIED));
}

void MockPermissionService::RequestPermissions(
    Vector<PermissionDescriptorPtr> permissions,
    bool user_gesture,
    mojom::blink::PermissionService::RequestPermissionsCallback) {
  NOTREACHED_IN_MIGRATION();
}

void MockPermissionService::RevokePermission(PermissionDescriptorPtr permission,
                                             RevokePermissionCallback) {
  NOTREACHED_IN_MIGRATION();
}

void MockPermissionService::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    mojom::blink::PermissionStatus last_known_status,
    mojo::PendingRemote<mojom::blink::PermissionObserver>) {
  NOTREACHED_IN_MIGRATION();
}

void MockPermissionService::AddPageEmbeddedPermissionObserver(
    PermissionDescriptorPtr permission,
    mojom::blink::PermissionStatus last_known_status,
    mojo::PendingRemote<mojom::blink::PermissionObserver>) {
  NOTREACHED_IN_MIGRATION();
}

void MockPermissionService::NotifyEventListener(
    PermissionDescriptorPtr permission,
    const String& event_type,
    bool is_added) {
  NOTREACHED_IN_MIGRATION();
}
// WakeLockTestingContext

WakeLockTestingContext::WakeLockTestingContext(
    MockWakeLockService* mock_wake_lock_service) {
  DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::WakeLockService::Name_,
      WTF::BindRepeating(&MockWakeLockService::BindRequest,
                         WTF::Unretained(mock_wake_lock_service)));
  DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::PermissionService::Name_,
      WTF::BindRepeating(&MockPermissionService::BindRequest,
                         WTF::Unretained(&permission_service_)));
}

WakeLockTestingContext::~WakeLockTestingContext() {
  // Remove the testing binder to avoid crashes between tests caused by
  // our mocks rebinding an already-bound Binding.
  // See https://crbug.com/1010116 for more information.
  DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::WakeLockService::Name_, {});
  DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::PermissionService::Name_, {});
}

LocalDOMWindow* WakeLockTestingContext::DomWindow() {
  return Frame()->DomWindow();
}

LocalFrame* WakeLockTestingContext::Frame() {
  return &testing_scope_.GetFrame();
}

ScriptState* WakeLockTestingContext::GetScriptState() {
  return testing_scope_.GetScriptState();
}

MockPermissionService& WakeLockTestingContext::GetPermissionService() {
  return permission_service_;
}

void WakeLockTestingContext::WaitForPromiseFulfillment(
    ScriptPromiseUntyped promise) {
  base::RunLoop run_loop;
  promise.Then(MakeGarbageCollected<ScriptFunction>(
      GetScriptState(),
      MakeGarbageCollected<ClosureRunnerCallable>(run_loop.QuitClosure())));
  // Execute pending microtasks, otherwise it can take a few seconds for the
  // promise to resolve.
  GetScriptState()->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
      GetScriptState()->GetIsolate());
  RunWithStack(&run_loop);
}

// Synchronously waits for |promise| to be rejected.
void WakeLockTestingContext::WaitForPromiseRejection(
    ScriptPromiseUntyped promise) {
  base::RunLoop run_loop;
  promise.Then(
      nullptr,
      MakeGarbageCollected<ScriptFunction>(
          GetScriptState(),
          MakeGarbageCollected<ClosureRunnerCallable>(run_loop.QuitClosure())));
  // Execute pending microtasks, otherwise it can take a few seconds for the
  // promise to resolve.
  GetScriptState()->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
      GetScriptState()->GetIsolate());
  RunWithStack(&run_loop);
}

// ScriptPromiseUtils

// static
v8::Promise::PromiseState ScriptPromiseUtils::GetPromiseState(
    const ScriptPromise<WakeLockSentinel>& promise) {
  return promise.V8Promise()->State();
}

// static
DOMException* ScriptPromiseUtils::GetPromiseResolutionAsDOMException(
    v8::Isolate* isolate,
    const ScriptPromise<WakeLockSentinel>& promise) {
  return V8DOMException::ToWrappable(isolate, promise.V8Promise()->Result());
}

// static
WakeLockSentinel* ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(
    v8::Isolate* isolate,
    const ScriptPromise<WakeLockSentinel>& promise) {
  return V8WakeLockSentinel::ToWrappable(isolate,
                                         promise.V8Promise()->Result());
}

}  // namespace blink
