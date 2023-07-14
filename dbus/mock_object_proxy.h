// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_MOCK_OBJECT_PROXY_H_
#define DBUS_MOCK_OBJECT_PROXY_H_

#include <memory>
#include <string>

#include "base/types/expected.h"
#include "dbus/error.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dbus {

// Mock for ObjectProxy.
class MockObjectProxy : public ObjectProxy {
 public:
  MockObjectProxy(Bus* bus,
                  const std::string& service_name,
                  const ObjectPath& object_path);

  MOCK_METHOD2(
      CallMethodAndBlock,
      base::expected<std::unique_ptr<Response>, Error>(MethodCall* method_call,
                                                       int timeout_ms));

  // This method is not mockable because it takes a move-only argument. To work
  // around this, CallMethod() implementation here calls DoCallMethod() which is
  // mockable.
  void CallMethod(MethodCall* method_call,
                  int timeout_ms,
                  ResponseCallback callback) override;
  MOCK_METHOD3(DoCallMethod,
               void(MethodCall* method_call,
                    int timeout_ms,
                    ResponseCallback* callback));

  // This method is not mockable because it takes a move-only argument. To work
  // around this, CallMethodWithErrorResponse() implementation here calls
  // DoCallMethodWithErrorResponse() which is mockable.
  void CallMethodWithErrorResponse(MethodCall* method_call,
                                   int timeout_ms,
                                   ResponseOrErrorCallback callback) override;
  MOCK_METHOD3(DoCallMethodWithErrorResponse,
               void(MethodCall* method_call,
                    int timeout_ms,
                    ResponseOrErrorCallback* callback));

  // This method is not mockable because it takes a move-only argument. To work
  // around this, CallMethodWithErrorCallback() implementation here calls
  // DoCallMethodWithErrorCallback() which is mockable.
  void CallMethodWithErrorCallback(MethodCall* method_call,
                                   int timeout_ms,
                                   ResponseCallback callback,
                                   ErrorCallback error_callback) override;
  MOCK_METHOD4(DoCallMethodWithErrorCallback,
               void(MethodCall* method_call,
                    int timeout_ms,
                    ResponseCallback* callback,
                    ErrorCallback* error_callback));

  // This method is not mockable because it takes a move-only argument. To work
  // around this, WaitForServiceToBeAvailable() implementation here calls
  // DoWaitForServiceToBeAvailable() which is mockable.
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  MOCK_METHOD1(DoWaitForServiceToBeAvailable,
               void(WaitForServiceToBeAvailableCallback* callback));

  // This method is not mockable because it takes a move-only argument. To work
  // around this, ConnectToSignal() implementation here calls
  // DoConnectToSignal() which is mockable.
  void ConnectToSignal(const std::string& interface_name,
                       const std::string& signal_name,
                       SignalCallback signal_callback,
                       OnConnectedCallback on_connected_callback) override;
  MOCK_METHOD4(DoConnectToSignal,
               void(const std::string& interface_name,
                    const std::string& signal_name,
                    SignalCallback signal_callback,
                    OnConnectedCallback* on_connected_callback));
  MOCK_METHOD1(SetNameOwnerChangedCallback,
               void(NameOwnerChangedCallback callback));
  MOCK_METHOD0(Detach, void());

 protected:
  ~MockObjectProxy() override;
};

}  // namespace dbus

#endif  // DBUS_MOCK_OBJECT_PROXY_H_
