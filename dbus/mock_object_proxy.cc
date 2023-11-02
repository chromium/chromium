// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/mock_object_proxy.h"

namespace dbus {

MockObjectProxy::MockObjectProxy(Bus* bus,
                                 const std::string& service_name,
                                 const ObjectPath& object_path)
    : ObjectProxy(bus, service_name, object_path, DEFAULT_OPTIONS) {
}

MockObjectProxy::~MockObjectProxy() = default;

void MockObjectProxy::CallMethod(MethodCall* method_call,
                                 int timeout_ms,
                                 ResponseCallback callback) {
  DoCallMethod(method_call, timeout_ms, &callback);
}

void MockObjectProxy::CallMethodWithErrorResponse(
    MethodCall* method_call,
    int timeout_ms,
    ResponseOrErrorCallback callback) {
  DoCallMethodWithErrorResponse(method_call, timeout_ms, &callback);
}

void MockObjectProxy::CallMethodWithErrorCallback(
    MethodCall* method_call,
    int timeout_ms,
    ResponseCallback callback,
    ErrorCallback error_callback) {
  DoCallMethodWithErrorCallback(method_call, timeout_ms, &callback,
                                &error_callback);
}

void MockObjectProxy::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  DoWaitForServiceToBeAvailable(&callback);
}

void MockObjectProxy::ConnectToSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    SignalCallback signal_callback,
    OnConnectedCallback on_connected_callback) {
  DoConnectToSignal(interface_name, signal_name, signal_callback,
                    &on_connected_callback);
}

}  // namespace dbus
