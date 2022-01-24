// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_manager_client.h"

#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {
namespace {
const std::vector<std::pair<int, bool>> kMockAdaptersAvailable = {{0, false},
                                                                  {5, true}};

class TestManagerObserver : public FlossManagerClient::Observer {
 public:
  explicit TestManagerObserver(FlossManagerClient* client) : client_(client) {
    client_->AddObserver(this);
  }

  ~TestManagerObserver() override { client_->RemoveObserver(this); }

  void ManagerPresent(bool present) override {
    manager_present_count_++;
    manager_present_ = present;
  }

  void AdapterPresent(int adapter, bool present) override {
    adapter_present_count_++;
    adapter_present_[adapter] = present;
  }

  void AdapterEnabledChanged(int adapter, bool enabled) override {
    adapter_enabled_changed_count_++;
    adapter_enabled_[adapter] = enabled;
  }

  int manager_present_count_ = 0;
  int adapter_present_count_ = 0;
  int adapter_enabled_changed_count_ = 0;

  bool manager_present_ = false;
  std::map<int, bool> adapter_present_;
  std::map<int, bool> adapter_enabled_;

 private:
  FlossManagerClient* client_ = nullptr;
};

}  // namespace

class FlossManagerClientTest : public testing::Test {
 public:
  FlossManagerClientTest() = default;

  void SetUpMocks() {
    auto obj_mgr_path =
        ::dbus::ObjectPath(FlossManagerClient::kObjectManagerPath);

    manager_object_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kManagerInterface, ::dbus::ObjectPath(kManagerObject));

    exported_callbacks_ = base::MakeRefCounted<::dbus::MockExportedObject>(
        bus_.get(),
        ::dbus::ObjectPath(FlossManagerClient::kExportedCallbacksPath));

    // Make sure a valid object manager is returned
    EXPECT_CALL(*bus_.get(), GetObjectProxy(kManagerInterface, obj_mgr_path))
        .WillOnce(::testing::Return(manager_object_proxy_.get()));
    EXPECT_CALL(*bus_.get(), GetDBusTaskRunner())
        .WillOnce(
            ::testing::Return(::base::SequencedTaskRunnerHandle::Get().get()));
    object_manager_ = ::dbus::ObjectManager::Create(
        bus_.get(), kManagerInterface, obj_mgr_path);
    EXPECT_CALL(*bus_.get(),
                GetObjectManager(kManagerInterface, dbus::ObjectPath("/")))
        .WillOnce(::testing::Return(object_manager_.get()));

    // Set up expects for remaining interfaces
    EXPECT_CALL(*bus_.get(), GetObjectProxy(kManagerInterface,
                                            ::dbus::ObjectPath(kManagerObject)))
        .WillRepeatedly(::testing::Return(manager_object_proxy_.get()));
    EXPECT_CALL(*bus_.get(), GetExportedObject)
        .WillRepeatedly(::testing::Return(exported_callbacks_.get()));
    EXPECT_CALL(*exported_callbacks_.get(), ExportMethod).Times(2);

    // Handle method calls on the object proxy
    ON_CALL(*manager_object_proxy_.get(), DoCallMethodWithErrorResponse)
        .WillByDefault(
            [this](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
              if (method_call->GetMember() == manager::kGetAvailableAdapters) {
                HandleGetAvailableAdapters(method_call, timeout_ms, cb);
              }

              method_called_[method_call->GetMember()]++;
            });
  }

  void SendHciDeviceCallback(int adapter,
                             bool present,
                             dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(manager::kCallbackInterface,
                                 manager::kOnHciDeviceChanged);
    method_call.SetSerial(serial_++);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(adapter);
    writer.AppendBool(present);

    client_->OnHciDeviceChange(&method_call, std::move(response));
  }

  void SendInvalidHciDeviceCallback(dbus::ExportedObject::ResponseSender rsp) {
    dbus::MethodCall method_call(manager::kCallbackInterface,
                                 manager::kOnHciDeviceChanged);
    method_call.SetSerial(serial_++);
    client_->OnHciDeviceChange(&method_call, std::move(rsp));
  }

  void SendHciEnabledCallback(int adapter,
                              bool enabled,
                              dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(manager::kCallbackInterface,
                                 manager::kOnHciEnabledChanged);
    method_call.SetSerial(serial_++);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(adapter);
    writer.AppendBool(enabled);

    client_->OnHciEnabledChange(&method_call, std::move(response));
  }

  void SendInvalidHciEnabledCallback(dbus::ExportedObject::ResponseSender rsp) {
    dbus::MethodCall method_call(manager::kCallbackInterface,
                                 manager::kOnHciEnabledChanged);
    method_call.SetSerial(serial_++);
    client_->OnHciEnabledChange(&method_call, std::move(rsp));
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    client_ = FlossManagerClient::Create();

    SetUpMocks();
  }

  void TearDown() override {
    // Clean up the client first so it gets rid of all its references to the
    // various buses, object proxies, etc.
    client_.reset();
    method_called_.clear();
  }

  void HandleGetAvailableAdapters(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    // Return that there are 2 adapter objects
    auto response = ::dbus::Response::CreateEmpty();

    ::dbus::MessageWriter msg(response.get());
    ::dbus::MessageWriter outer(nullptr);
    msg.OpenArray("a{sv}", &outer);
    for (auto kv : kMockAdaptersAvailable) {
      ::dbus::MessageWriter inner(nullptr);
      ::dbus::MessageWriter dict(nullptr);
      outer.OpenArray("{sv}", &inner);

      inner.OpenDictEntry(&dict);
      dict.AppendString("hci_interface");
      dict.AppendVariantOfInt32(kv.first);
      inner.CloseContainer(&dict);

      inner.OpenDictEntry(&dict);
      dict.AppendString("enabled");
      dict.AppendVariantOfBool(kv.second);
      inner.CloseContainer(&dict);

      outer.CloseContainer(&inner);
    }
    msg.CloseContainer(&outer);

    std::move(*cb).Run(response.get(), nullptr);
  }

  void ExpectErrorResponse(std::unique_ptr<dbus::Response> response) {
    EXPECT_EQ(response->GetMessageType(),
              dbus::Message::MessageType::MESSAGE_ERROR);
  }

  void ExpectNormalResponse(std::unique_ptr<dbus::Response> response) {
    EXPECT_NE(response->GetMessageType(),
              dbus::Message::MessageType::MESSAGE_ERROR);
  }

  void TriggerObjectAdded(dbus::ObjectPath& path,
                          const std::string& interface) {
    client_->ObjectAdded(path, interface);
  }

  void TriggerObjectRemoved(dbus::ObjectPath& path,
                            const std::string& interface) {
    client_->ObjectRemoved(path, interface);
  }

  bool IsInterfaceRegisteredOnObjectManager(const std::string& interface) {
    return client_->object_manager_ != nullptr &&
           client_->object_manager_->IsInterfaceRegisteredForTesting(interface);
  }

 protected:
  // DBus messages require an increasing serial number or the dbus libraries
  // assert.
  int serial_ = 1;
  std::unique_ptr<FlossManagerClient> client_;
  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockExportedObject> exported_callbacks_;
  scoped_refptr<::dbus::MockObjectProxy> manager_object_proxy_;
  scoped_refptr<::dbus::ObjectManager> object_manager_;
  std::map<std::string, int> method_called_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossManagerClientTest> weak_ptr_factory_{this};
};

// Make sure adapter presence is updated on init
TEST_F(FlossManagerClientTest, QueriesAdapterPresenceOnInit) {
  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_path=*/std::string());
  EXPECT_EQ(observer.manager_present_count_, 1);
  EXPECT_TRUE(observer.manager_present_);

  EXPECT_THAT(client_->GetAdapters(), ::testing::ElementsAre(0, 5));
  EXPECT_TRUE(client_->GetAdapterPresent(0));
  EXPECT_TRUE(client_->GetAdapterPresent(5));
  EXPECT_FALSE(client_->GetAdapterPresent(1));

  EXPECT_EQ(observer.adapter_present_count_, 2);
  EXPECT_TRUE(observer.adapter_present_[0]);
  EXPECT_TRUE(observer.adapter_present_[5]);
}

// Make sure adapter presence is plumbed through callbacks
TEST_F(FlossManagerClientTest, VerifyAdapterPresent) {
  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_path=*/std::string());
  EXPECT_EQ(observer.adapter_present_count_, 2);
  EXPECT_EQ(observer.adapter_enabled_changed_count_, 2);
  EXPECT_TRUE(observer.adapter_present_[0]);

  SendInvalidHciDeviceCallback(
      base::BindOnce(&FlossManagerClientTest::ExpectErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  SendHciDeviceCallback(
      0, false,
      base::BindOnce(&FlossManagerClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_THAT(client_->GetAdapters(), ::testing::ElementsAre(5));
  EXPECT_TRUE(client_->GetAdapterPresent(5));
  EXPECT_FALSE(client_->GetAdapterPresent(0));
  EXPECT_FALSE(client_->GetAdapterEnabled(0));

  // 2 to start and 1 to remove "0"
  EXPECT_EQ(observer.adapter_present_count_, 3);
  EXPECT_FALSE(observer.adapter_present_[0]);

  // On present = false, the client may not be sent an additional enabled
  // = false. It is implied and the client must act accordingly.
  EXPECT_EQ(observer.adapter_enabled_changed_count_, 2);
}

// Make sure adapter powered is plumbed through callbacks
TEST_F(FlossManagerClientTest, VerifyAdapterEnabled) {
  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_path=*/std::string());
  // Pre-conditions
  EXPECT_FALSE(client_->GetAdapterEnabled(0));
  EXPECT_TRUE(client_->GetAdapterEnabled(5));
  EXPECT_EQ(observer.adapter_enabled_changed_count_, 2);

  // Adapter 1 is not present at the beginning
  EXPECT_FALSE(client_->GetAdapterEnabled(1));
  EXPECT_FALSE(client_->GetAdapterPresent(1));

  SendInvalidHciEnabledCallback(
      base::BindOnce(&FlossManagerClientTest::ExpectErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  SendHciEnabledCallback(
      0, true,
      base::BindOnce(&FlossManagerClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_TRUE(client_->GetAdapterEnabled(0));
  EXPECT_EQ(observer.adapter_enabled_changed_count_, 3);
  EXPECT_TRUE(observer.adapter_enabled_[0]);

  // Enabled implies presence too
  SendHciEnabledCallback(
      1, true,
      base::BindOnce(&FlossManagerClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(client_->GetAdapterPresent(1));
  EXPECT_TRUE(client_->GetAdapterEnabled(1));

  EXPECT_EQ(observer.adapter_enabled_changed_count_, 4);
  EXPECT_TRUE(observer.adapter_enabled_[1]);
  // On enabled = true, present = true is implied. The platform should emit both
  // but the client shouldn't depend on it.
  EXPECT_FALSE(observer.adapter_present_[1]);

  // 5 was unchanged
  EXPECT_TRUE(client_->GetAdapterEnabled(5));
  EXPECT_TRUE(observer.adapter_enabled_[5]);
}

// Make sure manager presence is correctly detected
TEST_F(FlossManagerClientTest, HandleManagerPresence) {
  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_path=*/std::string());
  dbus::ObjectPath opath = dbus::ObjectPath(kManagerObject);
  EXPECT_EQ(observer.manager_present_count_, 1);

  // Make sure we registered against ObjectManager
  EXPECT_TRUE(IsInterfaceRegisteredOnObjectManager(kManagerInterface));

  // By default, the manager should be available
  EXPECT_TRUE(observer.manager_present_);

  // Triggering an ObjectRemoved should clear all present adapters
  TriggerObjectRemoved(opath, kManagerInterface);
  EXPECT_THAT(client_->GetAdapters(), ::testing::ElementsAre());
  EXPECT_FALSE(observer.manager_present_);

  // Triggering ObjectRemoved while already unavailable should do nothing
  TriggerObjectRemoved(opath, kManagerInterface);
  EXPECT_THAT(client_->GetAdapters(), ::testing::ElementsAre());
  EXPECT_FALSE(observer.manager_present_);

  method_called_.clear();
  // Triggering ObjectAdded should refill available hci devices and register
  // callbacks
  TriggerObjectAdded(opath, kManagerInterface);
  EXPECT_TRUE(method_called_[manager::kGetAvailableAdapters] > 0);
  EXPECT_TRUE(method_called_[manager::kRegisterCallback] > 0);
  EXPECT_TRUE(observer.manager_present_);

  // Clear present count to confirm a RemoveManager + RegisterManager occurred
  observer.manager_present_count_ = 0;

  // TODO(b/193839304) - Triggering ObjectAdded on an already added object
  //                     should trigger a remove and then re-add
  method_called_.clear();
  SendHciDeviceCallback(
      1, true,
      base::BindOnce(&FlossManagerClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(client_->GetAdapterPresent(1));
  TriggerObjectAdded(opath, kManagerInterface);
  // ManagerPresent should be called once for remove and once for register.
  EXPECT_EQ(observer.manager_present_count_, 2);
  EXPECT_FALSE(client_->GetAdapterPresent(1));  // Cleared previous adapter list
  EXPECT_TRUE(method_called_[manager::kGetAvailableAdapters] > 0);
  EXPECT_TRUE(method_called_[manager::kRegisterCallback] > 0);
}
}  // namespace floss
