// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_manager_client.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
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

using testing::DoAll;

const std::vector<std::pair<int, bool>> kMockAdaptersAvailable = {{0, false},
                                                                  {5, true}};

void FakeExportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    const dbus::ExportedObject::MethodCallCallback& method_call_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback) {
  std::move(on_exported_callback)
      .Run(interface_name, method_name, /*success=*/true);
}

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
  raw_ptr<FlossManagerClient> client_ = nullptr;
};

}  // namespace

class FlossManagerClientTest : public testing::Test {
 public:
  FlossManagerClientTest() = default;

  base::Version GetCurrVersion() {
    return floss::version::GetMaximalSupportedVersion();
  }

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
        .WillOnce(::testing::Return(
            ::base::SequencedTaskRunner::GetCurrentDefault().get()));
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

    // Exported callback methods that we don't need to invoke.
    EXPECT_CALL(*exported_callbacks_.get(), ExportMethod)
        .Times(1)
        .WillRepeatedly(&FakeExportMethod);
    // Save method handlers of exported callbacks that we need to invoke here.
    EXPECT_CALL(
        *exported_callbacks_.get(),
        ExportMethod(manager::kCallbackInterface, manager::kOnHciDeviceChanged,
                     testing::_, testing::_))
        .WillOnce(DoAll(testing::SaveArg<2>(&on_hci_device_changed_),
                        &FakeExportMethod));
    EXPECT_CALL(
        *exported_callbacks_.get(),
        ExportMethod(manager::kCallbackInterface, manager::kOnHciEnabledChanged,
                     testing::_, testing::_))
        .WillOnce(DoAll(testing::SaveArg<2>(&on_hci_enabled_changed_),
                        &FakeExportMethod));

    // Handle method calls on the object proxy
    ON_CALL(*manager_object_proxy_.get(), DoCallMethodWithErrorResponse)
        .WillByDefault([this](
                           ::dbus::MethodCall* method_call, int timeout_ms,
                           ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
          if (method_call->GetMember() == manager::kGetAvailableAdapters) {
            HandleGetAvailableAdapters(method_call, timeout_ms, cb);
          } else if (method_call->GetMember() == manager::kGetAdapterEnabled) {
            HandleGetAdapterEnabled(method_call, timeout_ms, cb);
          } else if (method_call->GetMember() == manager::kSetFlossEnabled) {
            HandleSetFlossEnabled(method_call, timeout_ms, cb);
          } else if (method_call->GetMember() == manager::kGetFlossEnabled) {
            HandleGetFlossEnabled(method_call, timeout_ms, cb);
          } else if (method_call->GetMember() == manager::kGetFlossApiVersion) {
            HandleGetFlossApiVersion(method_call, timeout_ms, cb);
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

    on_hci_device_changed_.Run(&method_call, std::move(response));
  }

  void SendInvalidHciDeviceCallback(dbus::ExportedObject::ResponseSender rsp) {
    dbus::MethodCall method_call(manager::kCallbackInterface,
                                 manager::kOnHciDeviceChanged);
    method_call.SetSerial(serial_++);
    on_hci_device_changed_.Run(&method_call, std::move(rsp));
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

    on_hci_enabled_changed_.Run(&method_call, std::move(response));
  }

  void SendInvalidHciEnabledCallback(dbus::ExportedObject::ResponseSender rsp) {
    dbus::MethodCall method_call(manager::kCallbackInterface,
                                 manager::kOnHciEnabledChanged);
    method_call.SetSerial(serial_++);
    on_hci_enabled_changed_.Run(&method_call, std::move(rsp));
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

  void HandleGetAdapterEnabled(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter writer(response.get());
    writer.AppendBool(get_adapter_enabled_return_);
    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleSetFlossEnabled(::dbus::MethodCall* method_call,
                             int timeout_ms,
                             ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    method_call->SetSerial(serial_++);
    if (fail_setfloss_count_ > 0) {
      fail_setfloss_count_--;

      std::string error_name("org.foo.bar");
      std::string error_message("SetFlossEnabled failed");
      auto error = ::dbus::ErrorResponse::FromMethodCall(
          method_call, error_name, error_message);
      std::move(*cb).Run(nullptr, error.get());
    } else {
      auto response = ::dbus::Response::CreateEmpty();
      std::move(*cb).Run(response.get(), nullptr);
    }
  }

  void HandleGetFlossEnabled(::dbus::MethodCall* method_call,
                             int timeout_ms,
                             ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    method_call->SetSerial(serial_++);
    if (fail_getfloss_count_ > 0) {
      fail_getfloss_count_--;

      std::string error_name("org.foo.bar");
      std::string error_message("GetFlossEnabled failed");
      auto error = ::dbus::ErrorResponse::FromMethodCall(
          method_call, error_name, error_message);
      std::move(*cb).Run(nullptr, error.get());
    } else {
      auto response = ::dbus::Response::CreateEmpty();
      ::dbus::MessageWriter writer(response.get());
      writer.AppendBool(floss_enabled_target_);
      std::move(*cb).Run(response.get(), nullptr);
    }
  }

  void HandleGetFlossApiVersion(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter writer(response.get());
    writer.AppendUint32(floss_api_version_);
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
  void SetFlossEnabled(bool enable,
                       int retry_count,
                       int retry_ms,
                       base::RepeatingClosure quitloop) {
    client_->SetFlossEnabled(enable, retry_count, retry_ms,
                             GetQuitLoopCallback(quitloop));
  }

  void DoGetFlossApiVersion() { client_->DoGetFlossApiVersion(); }

  bool IsCompatibleFlossApi() { return client_->IsCompatibleFlossApi(); }

  void EndRunLoopCallback(base::RepeatingClosure quit, DBusResult<bool> ret) {
    std::move(quit).Run();
  }

  ResponseCallback<bool> GetQuitLoopCallback(base::RepeatingClosure quit) {
    return base::BindOnce(&FlossManagerClientTest::EndRunLoopCallback,
                          weak_ptr_factory_.GetWeakPtr(), std::move(quit));
  }

  // DBus messages require an increasing serial number or the dbus libraries
  // assert.
  int serial_ = 1;
  std::unique_ptr<FlossManagerClient> client_;
  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockExportedObject> exported_callbacks_;
  scoped_refptr<::dbus::MockObjectProxy> manager_object_proxy_;
  scoped_refptr<::dbus::ObjectManager> object_manager_;
  std::map<std::string, int> method_called_;

  // Testing the |SetFlossEnabled| retry with a target of enabling Floss.
  int fail_setfloss_count_ = 0;
  int fail_getfloss_count_ = 0;
  bool floss_enabled_target_ = true;
  uint32_t floss_api_version_ = 0x1234abcd;

  bool get_adapter_enabled_return_ = false;

  dbus::ExportedObject::MethodCallCallback on_hci_device_changed_;
  dbus::ExportedObject::MethodCallCallback on_hci_enabled_changed_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossManagerClientTest> weak_ptr_factory_{this};
};

// Make sure adapter presence is updated on init
TEST_F(FlossManagerClientTest, QueriesAdapterPresenceOnInit) {
  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_index=*/-1,
                GetCurrVersion(), base::DoNothing());
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
  client_->Init(bus_.get(), kManagerInterface, /*adapter_index=*/-1,
                GetCurrVersion(), base::DoNothing());
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

// Make sure we query the enabled state when adapter presents
TEST_F(FlossManagerClientTest, VerifyAdapterPresentEnabled) {
  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_index=*/-1,
                GetCurrVersion(), base::DoNothing());

  EXPECT_EQ(method_called_[manager::kGetAdapterEnabled], 0);
  EXPECT_EQ(observer.adapter_present_count_, 2);

  // A disabled adapter presents
  SendHciDeviceCallback(
      1, true,
      base::BindOnce(&FlossManagerClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(method_called_[manager::kGetAdapterEnabled], 1);
  EXPECT_EQ(observer.adapter_present_count_, 3);
  EXPECT_FALSE(client_->GetAdapterEnabled(1));

  // An enabled adapter presents
  get_adapter_enabled_return_ = true;
  SendHciDeviceCallback(
      2, true,
      base::BindOnce(&FlossManagerClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(method_called_[manager::kGetAdapterEnabled], 2);
  EXPECT_EQ(observer.adapter_present_count_, 4);
  EXPECT_TRUE(client_->GetAdapterEnabled(2));

  // Presenting twice should be no-op
  SendHciDeviceCallback(
      2, true,
      base::BindOnce(&FlossManagerClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(method_called_[manager::kGetAdapterEnabled], 2);
  EXPECT_EQ(observer.adapter_present_count_, 4);
  EXPECT_TRUE(client_->GetAdapterEnabled(2));
}

// Make sure adapter powered is plumbed through callbacks
TEST_F(FlossManagerClientTest, VerifyAdapterEnabled) {
  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_index=*/-1,
                GetCurrVersion(), base::DoNothing());
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
  client_->Init(bus_.get(), kManagerInterface, /*adapter_index=*/-1,
                GetCurrVersion(), base::DoNothing());
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

  // Triggering ObjectAdded on an already added object should do nothing
  observer.manager_present_count_ = 0;
  method_called_.clear();
  SendHciDeviceCallback(
      1, true,
      base::BindOnce(&FlossManagerClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(method_called_[manager::kGetAdapterEnabled] > 0);
  EXPECT_TRUE(client_->GetAdapterPresent(1));
  TriggerObjectAdded(opath, kManagerInterface);
  EXPECT_EQ(observer.manager_present_count_, 0);
  EXPECT_TRUE(client_->GetAdapterPresent(1));
  EXPECT_TRUE(method_called_[manager::kGetAvailableAdapters] == 0);
  EXPECT_TRUE(method_called_[manager::kRegisterCallback] == 0);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(FlossManagerClientTest, SetFlossEnabledRetries) {
  base::RunLoop loop;

  TestManagerObserver observer(client_.get());
  floss_enabled_target_ = false;
  client_->Init(bus_.get(), kManagerInterface, /*adapter_index=*/-1,
                GetCurrVersion(), base::DoNothing());

  // First confirm we had it set to False
  EXPECT_EQ(method_called_[manager::kSetFlossEnabled], 1);
  EXPECT_EQ(method_called_[manager::kGetFlossEnabled], 1);

  method_called_.clear();

  // Retries up to 3 times across both Get and Set.
  fail_setfloss_count_ = 1;
  fail_getfloss_count_ = 1;
  floss_enabled_target_ = true;
  SetFlossEnabled(true, 3, 0, loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(method_called_[manager::kSetFlossEnabled], 2);
  EXPECT_EQ(method_called_[manager::kGetFlossEnabled], 2);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(FlossManagerClientTest, GetFlossApiVersion) {
  base::Version version = floss::version::IntoVersion(floss_api_version_);

  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_index=*/-1,
                GetCurrVersion(), base::DoNothing());

  method_called_.clear();
  DoGetFlossApiVersion();

  EXPECT_EQ(method_called_[manager::kGetFlossApiVersion], 1);
  EXPECT_EQ(client_->GetFlossApiVersion(), version);
}

TEST_F(FlossManagerClientTest, NewFlossDaemonIsNotCompatible) {
  // Given Floss daemon's Floss API version is a newer one.
  floss_api_version_ = 0xffffffff;

  // When FlossManagerClient gets the Floss API version at initialized.
  TestManagerObserver observer(client_.get());
  client_->Init(bus_.get(), kManagerInterface, /*adapter_index=*/-1,
                GetCurrVersion(), base::DoNothing());

  // Then, the Floss API exported by Floss daemon is not compatible.
  EXPECT_FALSE(IsCompatibleFlossApi());
}
}  // namespace floss
