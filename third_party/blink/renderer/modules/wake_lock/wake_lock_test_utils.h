// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TEST_UTILS_H_

#include "base/callback.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/wake_lock.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "v8/include/v8.h"

namespace blink {

class DOMException;
class Document;
class WakeLockSentinel;

// Mock WakeLock implementation that tracks whether it's bound or acquired, and
// provides a few helper methods to synchronously wait for RequestWakeLock()
// and CancelWakeLock() to be called.
class MockWakeLock : public device::mojom::blink::WakeLock {
 public:
  MockWakeLock();
  ~MockWakeLock() override;

  bool is_acquired() const { return is_acquired_; }

  void Bind(mojo::PendingReceiver<device::mojom::blink::WakeLock> receiver);

  // Forcefully terminate a binding to test connection errors.
  void Unbind();

  // Synchronously wait for RequestWakeLock() to be called.
  void WaitForRequest();

  // Synchronously wait for CancelWakeLock() to be called.
  void WaitForCancelation();

 private:
  void OnConnectionError();

  // device::mojom::blink::WakeLock implementation
  void RequestWakeLock() override;
  void CancelWakeLock() override;
  void AddClient(
      mojo::PendingReceiver<device::mojom::blink::WakeLock>) override;
  void ChangeType(device::mojom::blink::WakeLockType,
                  ChangeTypeCallback) override;
  void HasWakeLockForTests(HasWakeLockForTestsCallback) override;

  bool is_acquired_ = false;

  base::OnceClosure request_wake_lock_callback_;
  base::OnceClosure cancel_wake_lock_callback_;

  mojo::Receiver<device::mojom::blink::WakeLock> receiver_{this};
};

// Mock WakeLockService implementation that creates a MockWakeLock in its
// GetWakeLock() implementation.
class MockWakeLockService : public mojom::blink::WakeLockService {
 public:
  MockWakeLockService();
  ~MockWakeLockService() override;

  void BindRequest(mojo::ScopedMessagePipeHandle handle);

  MockWakeLock& get_wake_lock(WakeLockType type);

 private:
  // mojom::blink::WakeLockService implementation
  void GetWakeLock(
      device::mojom::blink::WakeLockType type,
      device::mojom::blink::WakeLockReason reason,
      const String& description,
      mojo::PendingReceiver<device::mojom::blink::WakeLock> receiver) override;

  MockWakeLock mock_wake_lock_[kWakeLockTypeCount];
  mojo::ReceiverSet<mojom::blink::WakeLockService> receivers_;
};

// Mock PermissionService implementation. It only implements the bits required
// by the Wake Lock code, and it mimics what we do in the content and chrome
// layers: screen locks are always granted, system locks are always denied.
class MockPermissionService final : public mojom::blink::PermissionService {
 public:
  MockPermissionService();
  ~MockPermissionService() override;

  void BindRequest(mojo::ScopedMessagePipeHandle handle);

  void SetPermissionResponse(WakeLockType, mojom::blink::PermissionStatus);

  void WaitForPermissionRequest(WakeLockType);

 private:
  bool GetWakeLockTypeFromDescriptor(
      const mojom::blink::PermissionDescriptorPtr& descriptor,
      WakeLockType* output);

  // mojom::blink::PermissionService implementation
  void HasPermission(mojom::blink::PermissionDescriptorPtr permission,
                     HasPermissionCallback) override;
  void RequestPermission(mojom::blink::PermissionDescriptorPtr permission,
                         bool user_gesture,
                         RequestPermissionCallback) override;
  void RequestPermissions(
      Vector<mojom::blink::PermissionDescriptorPtr> permissions,
      bool user_gesture,
      RequestPermissionsCallback) override;
  void RevokePermission(mojom::blink::PermissionDescriptorPtr permission,
                        RevokePermissionCallback) override;
  void AddPermissionObserver(
      mojom::blink::PermissionDescriptorPtr permission,
      mojom::blink::PermissionStatus last_known_status,
      mojo::PendingRemote<mojom::blink::PermissionObserver>) override;

  void OnConnectionError();

  mojo::Receiver<mojom::blink::PermissionService> receiver_{this};

  base::Optional<mojom::blink::PermissionStatus>
      permission_responses_[kWakeLockTypeCount];

  base::OnceClosure request_permission_callbacks_[kWakeLockTypeCount];
};

// Overrides requests for WakeLockService with MockWakeLockService instances.
//
// Usage:
// TEST(Foo, Bar) {
//   MockWakeLockService mock_service;
//   WakeLockTestingContext context(&mock_service);
//   mojo::Remote<mojom::blink::WakeLockService> service;
//   context.GetDocument()->GetBrowserInterfaceBroker().GetInterface(
//       service.BindNewPipeAndPassReceiver());
//   service->GetWakeLock(...);  // Will call mock_service.GetWakeLock().
// }
class WakeLockTestingContext final {
  STACK_ALLOCATED();

 public:
  WakeLockTestingContext(MockWakeLockService* mock_wake_lock_service);
  ~WakeLockTestingContext();

  Document* GetDocument();
  LocalFrame* Frame();
  ScriptState* GetScriptState();
  MockPermissionService& GetPermissionService();

  // Synchronously waits for |promise| to be fulfilled.
  ScriptPromise WaitForPromiseFulfillment(ScriptPromise promise);

  // Synchronously waits for |promise| to be rejected.
  void WaitForPromiseRejection(ScriptPromise promise);

 private:
  MockPermissionService permission_service_;
  V8TestingScope testing_scope_;
};

// Utility functions to retrieve promise data out of a ScriptPromise.
class ScriptPromiseUtils final {
 public:
  // Shorthand for getting a PromiseState out of a ScriptPromise.
  static v8::Promise::PromiseState GetPromiseState(
      const ScriptPromise& promise);

  // Shorthand for getting a DOMException* out of a ScriptPromise. This assumes
  // the promise has been resolved with a DOMException. If the conversion fails,
  // nullptr is returned.
  static DOMException* GetPromiseResolutionAsDOMException(const ScriptPromise&);

  // Shorthand for getting a WakeLockSentinel* out of a ScriptPromise. This
  // assumes the promise has been resolved with a WakeLockSentinel. If the
  // conversion fails, nullptr is returned.
  static WakeLockSentinel* GetPromiseResolutionAsWakeLockSentinel(
      const ScriptPromise&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TEST_UTILS_H_
