// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_MOCK_BUS_H_
#define DBUS_MOCK_BUS_H_

#include <stdint.h>

#include "base/task/sequenced_task_runner.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dbus {

// Mock for Bus class. Along with MockObjectProxy and MockExportedObject,
// the mock classes can be used to write unit tests without issuing real
// D-Bus calls.
class MockBus : public Bus {
 public:
  MockBus(const Bus::Options& options);

  MOCK_METHOD2(GetObjectProxy, ObjectProxy*(const std::string& service_name,
                                            const ObjectPath& object_path));
  MOCK_METHOD3(GetObjectProxyWithOptions,
               ObjectProxy*(const std::string& service_name,
                            const ObjectPath& object_path,
                            int options));
  MOCK_METHOD1(GetExportedObject, ExportedObject*(
      const ObjectPath& object_path));
  MOCK_METHOD2(GetObjectManager, ObjectManager*(const std::string&,
                                                const ObjectPath&));
  MOCK_METHOD0(ShutdownAndBlock, void());
  MOCK_METHOD0(ShutdownOnDBusThreadAndBlock, void());
  MOCK_METHOD0(Connect, bool());
  MOCK_METHOD3(RequestOwnership, void(
      const std::string& service_name,
      ServiceOwnershipOptions options,
      OnOwnershipCallback on_ownership_callback));
  MOCK_METHOD2(RequestOwnershipAndBlock, bool(const std::string& service_name,
                                              ServiceOwnershipOptions options));
  MOCK_METHOD1(ReleaseOwnership, bool(const std::string& service_name));
  MOCK_METHOD0(SetUpAsyncOperations, bool());
  MOCK_METHOD2(
      SendWithReplyAndBlock,
      base::expected<std::unique_ptr<Response>, Error>(DBusMessage* request,
                                                       int timeout_ms));
  MOCK_METHOD3(SendWithReply, void(DBusMessage* request,
                                   DBusPendingCall** pending_call,
                                   int timeout_ms));
  MOCK_METHOD2(Send, void(DBusMessage* request, uint32_t* serial));
  MOCK_METHOD2(AddFilterFunction,
               void(DBusHandleMessageFunction filter_function,
                    void* user_data));
  MOCK_METHOD2(RemoveFilterFunction,
               void(DBusHandleMessageFunction filter_function,
                    void* user_data));
  MOCK_METHOD2(AddMatch, void(const std::string& match_rule, Error* error));
  MOCK_METHOD2(RemoveMatch, bool(const std::string& match_rule, Error* error));
  MOCK_METHOD4(TryRegisterObjectPath,
               bool(const ObjectPath& object_path,
                    const DBusObjectPathVTable* vtable,
                    void* user_data,
                    Error* error));
  MOCK_METHOD4(TryRegisterFallback,
               bool(const ObjectPath& object_path,
                    const DBusObjectPathVTable* vtable,
                    void* user_data,
                    Error* error));
  MOCK_METHOD1(UnregisterObjectPath, void(const ObjectPath& object_path));
  MOCK_METHOD0(GetDBusTaskRunner, base::SequencedTaskRunner*());
  MOCK_METHOD0(GetOriginTaskRunner, base::SequencedTaskRunner*());
  MOCK_METHOD0(HasDBusThread, bool());
  MOCK_METHOD0(AssertOnOriginThread, void());
  MOCK_METHOD0(AssertOnDBusThread, void());
  MOCK_METHOD0(GetConnectionName, std::string());
  MOCK_METHOD0(IsConnected, bool());

 protected:
  ~MockBus() override;
};

}  // namespace dbus

#endif  // DBUS_MOCK_BUS_H_
