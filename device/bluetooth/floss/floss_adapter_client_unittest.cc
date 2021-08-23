// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_adapter_client.h"

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
#include "dbus/object_path.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {
namespace {

class TestAdapterObserver : public FlossAdapterClient::Observer {
 public:
  explicit TestAdapterObserver(FlossAdapterClient* client) : client_(client) {
    client_->AddObserver(this);
  }

  ~TestAdapterObserver() override { client_->RemoveObserver(this); }

  void AdapterAddressChanged(const std::string& address) override {
    address_changed_count_++;
    address_ = address;
  }

  void AdapterDiscoveringChanged(bool state) override {
    discovering_changed_count_++;
    discovering_state_ = state;
  }

  void AdapterFoundDevice(const FlossDeviceId& device_found) override {
    found_device_count_++;
    found_device_ = device_found;
  }

  void AdapterSspRequest(const FlossDeviceId& remote_device,
                         uint32_t cod,
                         FlossAdapterClient::BluetoothSspVariant variant,
                         uint32_t passkey) override {
    ssp_request_count_++;

    ssp_device_ = remote_device;
    cod_ = cod;
    variant_ = variant;
    passkey_ = passkey;
  }

  std::string address_;
  bool discovering_state_ = false;
  FlossDeviceId found_device_;

  FlossDeviceId ssp_device_;
  uint32_t cod_ = 0;
  FlossAdapterClient::BluetoothSspVariant variant_ =
      FlossAdapterClient::BluetoothSspVariant::kPasskeyConfirmation;
  uint32_t passkey_ = 0;

  int address_changed_count_ = 0;
  int discovering_changed_count_ = 0;
  int found_device_count_ = 0;
  int ssp_request_count_ = 0;

 private:
  FlossAdapterClient* client_ = nullptr;
};

}  // namespace

class FlossAdapterClientTest : public testing::Test {
 public:
  FlossAdapterClientTest() = default;

  void SetUpMocks() {
    adapter_path_ = FlossManagerClient::GenerateAdapterPath(adapter_index_);
    adapter_object_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kAdapterInterface, adapter_path_);
    exported_callbacks_ = base::MakeRefCounted<::dbus::MockExportedObject>(
        bus_.get(),
        ::dbus::ObjectPath(FlossAdapterClient::kExportedCallbacksPath));

    EXPECT_CALL(*bus_.get(), GetObjectProxy(kAdapterInterface, adapter_path_))
        .WillRepeatedly(::testing::Return(adapter_object_proxy_.get()));
    EXPECT_CALL(*bus_.get(), GetExportedObject)
        .WillRepeatedly(::testing::Return(exported_callbacks_.get()));

    // Make sure we export all callbacks. This will need to be updated once new
    // callbacks are added.
    EXPECT_CALL(*exported_callbacks_.get(), ExportMethod).Times(4);

    // Handle method calls on the object proxy
    ON_CALL(*adapter_object_proxy_.get(), DoCallMethodWithErrorResponse)
        .WillByDefault(
            [this](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
              if (method_call->GetMember() == adapter::kGetAddress) {
                HandleGetAddress(method_call, timeout_ms, cb);
              } else if (method_call->GetMember() == adapter::kCreateBond) {
                HandleCreateBond(method_call, timeout_ms, cb);
              }

              method_called_[method_call->GetMember()] = true;
            });
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    client_ = FlossAdapterClient::Create();

    valid_create_bond_ = false;
    SetUpMocks();
  }

  void TearDown() override {
    // Clean up the client first so it gets rid of all its references to the
    // various buses, object proxies, etc.
    client_.reset();
    method_called_.clear();
  }

  void ExpectErrorResponse(std::unique_ptr<dbus::Response> response) {
    EXPECT_EQ(response->GetMessageType(),
              dbus::Message::MessageType::MESSAGE_ERROR);
  }

  void ExpectNormalResponse(std::unique_ptr<dbus::Response> response) {
    EXPECT_NE(response->GetMessageType(),
              dbus::Message::MessageType::MESSAGE_ERROR);
  }

  void HandleGetAddress(::dbus::MethodCall* method_call,
                        int timeout_ms,
                        ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());
    msg.AppendString(adapter_address_);

    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleCreateBond(::dbus::MethodCall* method_call,
                        int timeout_ms,
                        ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    dbus::MessageReader msg(method_call);
    FlossDeviceId foo;
    uint32_t transport;
    valid_create_bond_ = FlossAdapterClient::ParseFlossDeviceId(&msg, &foo) &&
                         msg.PopUint32(&transport);

    auto response = ::dbus::Response::CreateEmpty();
    std::move(*cb).Run(response.get(), nullptr);
  }

  void ExpectValidCreateBond(const absl::optional<Error>& err) {
    EXPECT_TRUE(valid_create_bond_);
  }

  void SendAddressChangeCallback(
      bool error,
      const std::string& address,
      dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(adapter::kCallbackInterface,
                                 adapter::kOnAddressChanged);
    method_call.SetSerial(serial_++);
    if (!error) {
      dbus::MessageWriter writer(&method_call);
      writer.AppendString(address);
    }

    client_->OnAddressChanged(&method_call, std::move(response));
  }

  void SendDiscoveringChangeCallback(
      bool error,
      bool state,
      dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(adapter::kCallbackInterface,
                                 adapter::kOnDiscoveringChanged);
    method_call.SetSerial(serial_++);
    if (!error) {
      dbus::MessageWriter writer(&method_call);
      writer.AppendBool(state);
    } else {
      dbus::MessageWriter writer(&method_call);
      writer.AppendString("garbage");
    }

    client_->OnDiscoveringChanged(&method_call, std::move(response));
  }

  void EncodeFlossDeviceId(dbus::MessageWriter* writer,
                           const FlossDeviceId& device_id,
                           bool include_required_keys,
                           bool include_extra_keys) {
    dbus::MessageWriter array(nullptr);
    dbus::MessageWriter dict(nullptr);

    writer->OpenArray("{sv}", &array);

    // Required keys for FlossDeviceId to parse correctly
    if (include_required_keys) {
      array.OpenDictEntry(&dict);
      dict.AppendString("name");
      dict.AppendVariantOfString(device_id.name);
      array.CloseContainer(&dict);

      array.OpenDictEntry(&dict);
      dict.AppendString("address");
      dict.AppendVariantOfString(device_id.address);
      array.CloseContainer(&dict);
    }

    // Extra keys shouldn't affect parsing
    if (include_extra_keys) {
      array.OpenDictEntry(&dict);
      dict.AppendString("ignored key");
      dict.AppendVariantOfString("ignored");
      array.CloseContainer(&dict);
    }

    writer->CloseContainer(&array);
  }

  void SendDeviceFoundCallback(bool error,
                               const FlossDeviceId& device_id,
                               dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(adapter::kCallbackInterface,
                                 adapter::kOnDeviceFound);
    method_call.SetSerial(serial_++);

    dbus::MessageWriter writer(&method_call);
    EncodeFlossDeviceId(&writer, device_id,
                        /*include_required_keys=*/!error,
                        /*include_extra_keys=*/true);

    client_->OnDeviceFound(&method_call, std::move(response));
  }

  void SendSspRequestCallback(bool error,
                              const FlossDeviceId& device_id,
                              uint32_t cod,
                              FlossAdapterClient::BluetoothSspVariant variant,
                              uint32_t passkey,
                              dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(adapter::kCallbackInterface,
                                 adapter::kOnSspRequest);
    method_call.SetSerial(serial_++);

    dbus::MessageWriter writer(&method_call);
    EncodeFlossDeviceId(&writer, device_id,
                        /*include_required_keys=*/!error,
                        /*include_extra_keys=*/false);
    if (!error) {
      writer.AppendUint32(cod);
      writer.AppendUint32(static_cast<uint32_t>(variant));
      writer.AppendUint32(passkey);
    }

    client_->OnSspRequest(&method_call, std::move(response));
  }

  int serial_ = 1;
  int adapter_index_ = 5;
  dbus::ObjectPath adapter_path_;
  std::string adapter_address_ = "00:11:22:33:44:55";

  bool valid_create_bond_;
  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockExportedObject> exported_callbacks_;
  scoped_refptr<::dbus::MockObjectProxy> adapter_object_proxy_;
  std::map<std::string, bool> method_called_;
  std::unique_ptr<FlossAdapterClient> client_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossAdapterClientTest> weak_ptr_factory_{this};
};

// Verify initial states and assumptions.
TEST_F(FlossAdapterClientTest, InitializesCorrectly) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_path_.value());

  EXPECT_TRUE(method_called_[adapter::kGetAddress]);
  EXPECT_TRUE(method_called_[adapter::kRegisterCallback]);

  // Make sure the address is initialized correctly
  EXPECT_EQ(test_observer.address_changed_count_, 1);
  EXPECT_EQ(client_->GetAddress(), adapter_address_);
}

TEST_F(FlossAdapterClientTest, HandlesAddressChanges) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_path_.value());
  EXPECT_EQ(test_observer.address_changed_count_, 1);

  SendAddressChangeCallback(
      /*error=*/true, /*address=*/std::string(),
      base::BindOnce(&FlossAdapterClientTest::ExpectErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.address_changed_count_, 1);

  std::string test_address("11:22:33:44:55:66");
  SendAddressChangeCallback(
      /*error=*/false, test_address,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.address_changed_count_, 2);
  EXPECT_EQ(test_observer.address_, test_address);
  EXPECT_EQ(client_->GetAddress(), test_address);
}

TEST_F(FlossAdapterClientTest, HandlesDiscoveryChanges) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_path_.value());
  EXPECT_EQ(test_observer.discovering_changed_count_, 0);

  SendDiscoveringChangeCallback(
      /*error=*/true, /*state=*/false,
      base::BindOnce(&FlossAdapterClientTest::ExpectErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.discovering_changed_count_, 0);
  EXPECT_FALSE(test_observer.discovering_state_);

  // Adapter client doesn't cache the discovering state and will just forward it
  // without checking if it's the same as before.
  SendDiscoveringChangeCallback(
      /*error=*/false, /*state=*/false,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(test_observer.discovering_changed_count_, 1);
  EXPECT_FALSE(test_observer.discovering_state_);

  SendDiscoveringChangeCallback(
      /*error=*/false, /*state=*/true,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(test_observer.discovering_changed_count_, 2);
  EXPECT_TRUE(test_observer.discovering_state_);
}

TEST_F(FlossAdapterClientTest, HandlesFoundDevices) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_path_.value());
  EXPECT_EQ(test_observer.found_device_count_, 0);

  FlossDeviceId device_id = {.address = "66:55:44:33:22:11", .name = "First"};

  SendDeviceFoundCallback(
      /*error=*/true, device_id,
      base::BindOnce(&FlossAdapterClientTest::ExpectErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.found_device_count_, 0);
  SendDeviceFoundCallback(
      /*error=*/false, device_id,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.found_device_count_, 1);
  EXPECT_EQ(test_observer.found_device_.name, device_id.name);
  EXPECT_EQ(test_observer.found_device_.address, device_id.address);
}

TEST_F(FlossAdapterClientTest, HandlesSsp) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_path_.value());
  EXPECT_EQ(test_observer.ssp_request_count_, 0);

  FlossDeviceId device_id = {.address = "11:22:33:66:55:44", .name = "Foobar"};
  auto variant = FlossAdapterClient::BluetoothSspVariant::kPasskeyNotification;
  uint32_t cod = 0x0c0dc0d0;
  uint32_t passkey = 0xc0dec0de;

  SendSspRequestCallback(
      /*error=*/true, device_id, cod, variant, passkey,
      base::BindOnce(&FlossAdapterClientTest::ExpectErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.ssp_request_count_, 0);
  SendSspRequestCallback(
      /*error=*/false, device_id, cod, variant, passkey,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.ssp_request_count_, 1);
  EXPECT_EQ(test_observer.ssp_device_.name, device_id.name);
  EXPECT_EQ(test_observer.ssp_device_.address, device_id.address);
  EXPECT_EQ(test_observer.cod_, cod);
  EXPECT_EQ(test_observer.variant_, variant);
  EXPECT_EQ(test_observer.passkey_, passkey);
}

TEST_F(FlossAdapterClientTest, CreateBond) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_path_.value());

  FlossDeviceId bond = {.address = "00:22:44:11:33:55", .name = "James"};
  auto transport = FlossAdapterClient::BluetoothTransport::kBrEdr;

  client_->CreateBond(
      base::BindOnce(&FlossAdapterClientTest::ExpectValidCreateBond,
                     weak_ptr_factory_.GetWeakPtr()),
      bond, transport);
}

}  // namespace floss
