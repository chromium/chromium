// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/floss/floss_adapter_client.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"
#include "device/bluetooth/floss/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

constexpr char kTestMethod0[] = "TestMethod0";
constexpr char kTestMethod1[] = "TestMethod1";
constexpr char kTestMethod2[] = "TestMethod2";

constexpr uint8_t kFakeU8Return = 100;
constexpr uint32_t kFakeU32Return = 20000;
constexpr char kFakeStrReturn[] = "fake return";
constexpr uint32_t kFakeU32Param = 30;
constexpr char kFakeStrParam[] = "fake param";
constexpr bool kFakeBoolParam = true;
constexpr uint32_t kFakeCallbackId = 23;
constexpr uint32_t kFakeConnectionCallbackId = 24;

constexpr char kFakeDeviceAddr[] = "11:22:33:44:55:66";
constexpr char kFakeDeviceName[] = "Some Device";
constexpr uint8_t kFakeBytes[] = {1, 1, 2, 3, 5, 8, 13};
constexpr uint8_t kFakeUuidByteArray[] = {0, 1, 2,  3,  4,  5,  6,  7,
                                          8, 9, 10, 11, 12, 13, 14, 15};
constexpr char kFakeUuidStr[] = "00010203-0405-0607-0809-0a0b0c0d0e0f";
constexpr floss::FlossAdapterClient::BluetoothDeviceType kFakeType =
    floss::FlossAdapterClient::BluetoothDeviceType::kBle;

}  // namespace

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

  void DiscoverableChanged(bool discoverable) override {
    discoverable_changed_count_++;
    discoverable_ = discoverable;
  }

  void AdapterDiscoveringChanged(bool state) override {
    discovering_changed_count_++;
    discovering_state_ = state;
  }

  void AdapterFoundDevice(const FlossDeviceId& device_found) override {
    found_device_count_++;
    found_device_ = device_found;
  }

  void AdapterClearedDevice(const FlossDeviceId& device_cleared) override {
    cleared_device_count_++;
    cleared_device_ = device_cleared;
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

  void AdapterPinDisplay(const FlossDeviceId& remote_device,
                         std::string pincode) override {
    pin_display_count_++;

    pin_display_device_ = remote_device;
    pincode_ = pincode;
  }

  void AdapterPinRequest(const FlossDeviceId& remote_device,
                         uint32_t cod,
                         bool min_16_digit) override {
    pin_request_count_++;

    pin_request_device_ = remote_device;
  }

  std::string address_;
  bool discoverable_;
  bool discovering_state_ = false;
  FlossDeviceId found_device_;
  FlossDeviceId cleared_device_;

  FlossDeviceId ssp_device_;
  uint32_t cod_ = 0;
  FlossAdapterClient::BluetoothSspVariant variant_ =
      FlossAdapterClient::BluetoothSspVariant::kPasskeyConfirmation;
  uint32_t passkey_ = 0;
  FlossDeviceId pin_display_device_;
  FlossDeviceId pin_request_device_;
  std::string pincode_;

  int address_changed_count_ = 0;
  int discoverable_changed_count_ = 0;
  int discovering_changed_count_ = 0;
  int found_device_count_ = 0;
  int cleared_device_count_ = 0;
  int ssp_request_count_ = 0;
  int pin_display_count_ = 0;
  int pin_request_count_ = 0;

 private:
  raw_ptr<FlossAdapterClient> client_ = nullptr;
};

}  // namespace

class FlossAdapterClientTest : public testing::Test {
 public:
  FlossAdapterClientTest() = default;

  base::Version GetCurrVersion() {
    return floss::version::GetMaximalSupportedVersion();
  }

  void SetUpMocks() {
    adapter_path_ = FlossDBusClient::GenerateAdapterPath(adapter_index_);
    adapter_object_proxy_ = base::MakeRefCounted<::dbus::MockObjectProxy>(
        bus_.get(), kAdapterInterface, adapter_path_);
    exported_callbacks_ = base::MakeRefCounted<::dbus::MockExportedObject>(
        bus_.get(),
        ::dbus::ObjectPath(FlossAdapterClient::kExportedCallbacksPath));

    EXPECT_CALL(*bus_.get(), GetObjectProxy(kAdapterInterface, adapter_path_))
        .WillRepeatedly(::testing::Return(adapter_object_proxy_.get()));
    EXPECT_CALL(*bus_.get(), GetExportedObject)
        .WillRepeatedly(::testing::Return(exported_callbacks_.get()));

    // Exported callback methods that we don't need to invoke.  This will need
    // to be updated once new callbacks are added.
    // TODO(b/233124093): Reduce this count by 2 when SDP tests are added.
    EXPECT_CALL(*exported_callbacks_.get(), ExportMethod).Times(13);

    // Save the method handlers of exported callbacks that we need to invoke in
    // test.
    EXPECT_CALL(
        *exported_callbacks_.get(),
        ExportMethod(adapter::kCallbackInterface, adapter::kOnAddressChanged,
                     testing::_, testing::_))
        .WillOnce(testing::SaveArg<2>(&on_address_changed_));
    EXPECT_CALL(*exported_callbacks_.get(),
                ExportMethod(adapter::kCallbackInterface,
                             adapter::kOnNameChanged, testing::_, testing::_))
        .WillOnce(testing::SaveArg<2>(&on_name_changed_));
    EXPECT_CALL(
        *exported_callbacks_.get(),
        ExportMethod(adapter::kCallbackInterface,
                     adapter::kOnDiscoverableChanged, testing::_, testing::_))
        .WillOnce(testing::SaveArg<2>(&on_discoverable_changed_));

    // Handle method calls on the object proxy
    ON_CALL(
        *adapter_object_proxy_.get(),
        DoCallMethodWithErrorResponse(HasMemberOf(adapter::kGetAddress), _, _))
        .WillByDefault(Invoke(this, &FlossAdapterClientTest::HandleGetAddress));
    ON_CALL(*adapter_object_proxy_.get(),
            DoCallMethodWithErrorResponse(HasMemberOf(adapter::kGetName), _, _))
        .WillByDefault(Invoke(this, &FlossAdapterClientTest::HandleGetName));
    ON_CALL(*adapter_object_proxy_.get(),
            DoCallMethodWithErrorResponse(
                HasMemberOf(adapter::kGetDiscoverable), _, _))
        .WillByDefault(
            Invoke(this, &FlossAdapterClientTest::HandleGetDiscoverable));
    ON_CALL(*adapter_object_proxy_.get(),
            DoCallMethodWithErrorResponse(
                HasMemberOf(adapter::kIsLeExtendedAdvertisingSupported), _, _))
        .WillByDefault(Invoke(
            this,
            &FlossAdapterClientTest::HandleIsLeExtendedAdvertisingSupported));
  }

  void SetUp() override {
    ::dbus::Bus::Options options;
    options.bus_type = ::dbus::Bus::BusType::SYSTEM;
    bus_ = base::MakeRefCounted<::dbus::MockBus>(options);
    client_ = FlossAdapterClient::Create();

    SetUpMocks();
  }

  void TearDown() override {
    // Clean up the client first so it gets rid of all its references to the
    // various buses, object proxies, etc.
    client_.reset();
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

  void HandleGetName(::dbus::MethodCall* method_call,
                     int timeout_ms,
                     ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());
    msg.AppendString(adapter_name_);

    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleGetDiscoverable(::dbus::MethodCall* method_call,
                             int timeout_ms,
                             ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());
    msg.AppendBool(adapter_discoverable_);

    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleIsLeExtendedAdvertisingSupported(
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    auto response = ::dbus::Response::CreateEmpty();
    ::dbus::MessageWriter msg(response.get());
    msg.AppendBool(ext_adv_supported_);

    std::move(*cb).Run(response.get(), nullptr);
  }

  void HandleCreateBond(
      FlossDeviceId expected_device,
      FlossAdapterClient::BluetoothTransport expected_transport,
      ::dbus::MethodCall* method_call,
      int timeout_ms,
      ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
    dbus::MessageReader reader(method_call);
    FlossDeviceId device;
    uint32_t transport;

    ASSERT_TRUE(
        FlossAdapterClient::ReadAllDBusParams(&reader, &device, &transport));
    EXPECT_EQ(expected_device, device);
    EXPECT_EQ(expected_transport,
              static_cast<FlossAdapterClient::BluetoothTransport>(transport));

    auto response = ::dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    bool kSuccess = true;
    writer.AppendBool(kSuccess);
    std::move(*cb).Run(response.get(), nullptr);
  }

  void ExpectValidCreateBond(DBusResult<bool> ret) {
    ASSERT_TRUE(ret.has_value());
    EXPECT_TRUE(*ret);
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

    on_address_changed_.Run(&method_call, std::move(response));
  }

  void SendNameChangeCallback(bool error,
                              const std::string& name,
                              dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(adapter::kCallbackInterface,
                                 adapter::kOnNameChanged);
    method_call.SetSerial(serial_++);
    if (!error) {
      dbus::MessageWriter writer(&method_call);
      writer.AppendString(name);
    }

    on_name_changed_.Run(&method_call, std::move(response));
  }

  void SendDiscoverableChangeCallback(
      bool error,
      bool discoverable,
      dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(adapter::kCallbackInterface,
                                 adapter::kOnDiscoverableChanged);
    method_call.SetSerial(serial_++);
    if (!error) {
      dbus::MessageWriter writer(&method_call);
      writer.AppendBool(discoverable);
    }

    on_discoverable_changed_.Run(&method_call, std::move(response));
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

  void SendDeviceClearedCallback(
      bool error,
      const FlossDeviceId& device_id,
      dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(adapter::kCallbackInterface,
                                 adapter::kOnDeviceFound);
    method_call.SetSerial(serial_++);

    dbus::MessageWriter writer(&method_call);
    EncodeFlossDeviceId(&writer, device_id,
                        /*include_required_keys=*/!error,
                        /*include_extra_keys=*/true);

    client_->OnDeviceCleared(&method_call, std::move(response));
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

  void SendAdapterPropertyChangedCallback(
      bool error,
      FlossAdapterClient::BtPropertyType type,
      dbus::ExportedObject::ResponseSender response) {
    dbus::MethodCall method_call(adapter::kCallbackInterface,
                                 adapter::kOnAdapterPropertyChanged);
    method_call.SetSerial(serial_++);

    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(static_cast<uint32_t>(type));

    client_->OnAdapterPropertyChanged(&method_call, std::move(response));
  }

  int serial_ = 1;
  int adapter_index_ = 5;
  dbus::ObjectPath adapter_path_;
  std::string adapter_address_ = "00:11:22:33:44:55";
  std::string adapter_name_ = "floss";
  bool adapter_discoverable_ = false;
  bool ext_adv_supported_ = true;

  scoped_refptr<::dbus::MockBus> bus_;
  scoped_refptr<::dbus::MockExportedObject> exported_callbacks_;
  scoped_refptr<::dbus::MockObjectProxy> adapter_object_proxy_;
  std::unique_ptr<FlossAdapterClient> client_;

  dbus::ExportedObject::MethodCallCallback on_address_changed_;
  dbus::ExportedObject::MethodCallCallback on_name_changed_;
  dbus::ExportedObject::MethodCallCallback on_discoverable_changed_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossAdapterClientTest> weak_ptr_factory_{this};
};

// Verify initial states and assumptions.
TEST_F(FlossAdapterClientTest, InitializesCorrectly) {
  TestAdapterObserver test_observer(client_.get());

  // Because of the specific method call expectations below, we need a catch all
  // here to say that it is okay to have more method calls of any sort (not
  // exclusively those specific calls).
  EXPECT_CALL(*adapter_object_proxy_.get(), DoCallMethodWithErrorResponse)
      .Times(testing::AnyNumber());

  // Expected specific method calls.
  EXPECT_CALL(
      *adapter_object_proxy_.get(),
      DoCallMethodWithErrorResponse(HasMemberOf(adapter::kGetAddress), _, _))
      .Times(1);
  EXPECT_CALL(
      *adapter_object_proxy_.get(),
      DoCallMethodWithErrorResponse(HasMemberOf(adapter::kGetName), _, _))
      .Times(1);
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kGetDiscoverable), _, _))
      .Times(1);
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kRegisterCallback), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        dbus::ObjectPath param1;
        ASSERT_TRUE(msg.PopObjectPath(&param1));
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with uint32_t return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(kFakeCallbackId);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kRegisterConnectionCallback), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        dbus::ObjectPath param1;
        ASSERT_TRUE(msg.PopObjectPath(&param1));
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with uint32_t return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(kFakeConnectionCallbackId);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });

  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Make sure the address is initialized correctly
  EXPECT_EQ(test_observer.address_changed_count_, 1);
  EXPECT_EQ(client_->GetAddress(), adapter_address_);

  // Make sure name is initialized correctly
  EXPECT_EQ(client_->GetName(), adapter_name_);

  // Make sure discoverable is initialized correctly
  EXPECT_EQ(test_observer.discoverable_changed_count_, 1);
  EXPECT_EQ(client_->GetDiscoverable(), adapter_discoverable_);

  // Make sure extended advertising support is initialized correctly.
  EXPECT_EQ(client_->IsExtAdvSupported(), ext_adv_supported_);

  // Make sure to unregister callbacks when client is destroyed
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kUnregisterCallback), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint32_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kFakeCallbackId, param1);
        EXPECT_FALSE(msg.HasMoreData());
      });
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kUnregisterConnectionCallback), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint32_t param1;
        ASSERT_TRUE(FlossDBusClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(kFakeConnectionCallbackId, param1);
        EXPECT_FALSE(msg.HasMoreData());
      });
}

TEST_F(FlossAdapterClientTest, HandlesAddressChanges) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());
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

TEST_F(FlossAdapterClientTest, HandlesNameChanges) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  std::string test_name("floss_test_name");
  SendNameChangeCallback(
      /*error=*/false, test_name,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(client_->GetName(), test_name);
}

TEST_F(FlossAdapterClientTest, HandlesDiscoverableChanges) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());
  EXPECT_EQ(test_observer.discoverable_changed_count_, 1);

  SendDiscoverableChangeCallback(
      /*error=*/true, /*discoverable=*/true,
      base::BindOnce(&FlossAdapterClientTest::ExpectErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.discoverable_changed_count_, 1);
  EXPECT_EQ(test_observer.discoverable_, false);
  EXPECT_EQ(client_->GetDiscoverable(), false);

  SendDiscoverableChangeCallback(
      /*error=*/false, /*discoverable=*/true,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.discoverable_changed_count_, 2);
  EXPECT_EQ(test_observer.discoverable_, true);
  EXPECT_EQ(client_->GetDiscoverable(), true);
}

TEST_F(FlossAdapterClientTest, HandlesDiscoveryChanges) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());
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
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());
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

TEST_F(FlossAdapterClientTest, HandlesClearedDevices) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());
  EXPECT_EQ(test_observer.cleared_device_count_, 0);

  FlossDeviceId device_id = {.address = "66:55:44:33:22:11", .name = "First"};

  SendDeviceClearedCallback(
      /*error=*/true, device_id,
      base::BindOnce(&FlossAdapterClientTest::ExpectErrorResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.cleared_device_count_, 0);
  SendDeviceClearedCallback(
      /*error=*/false, device_id,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(test_observer.cleared_device_count_, 1);
  EXPECT_EQ(test_observer.cleared_device_.name, device_id.name);
  EXPECT_EQ(test_observer.cleared_device_.address, device_id.address);
}

// Block the event in LaCrOS so it won't race with AshChrome. See b/308988818.
// TODO(b/274706838): Redesign DBus API so it's only received by the correct
// client.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(FlossAdapterClientTest, HandlesSsp) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());
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
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(FlossAdapterClientTest, CreateBond) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  FlossDeviceId bond = {.address = "00:22:44:11:33:55", .name = "James"};
  auto transport = FlossAdapterClient::BluetoothTransport::kBrEdr;

  EXPECT_CALL(
      *adapter_object_proxy_.get(),
      DoCallMethodWithErrorResponse(HasMemberOf(adapter::kCreateBond), _, _))
      .WillOnce([this, &bond, &transport](
                    ::dbus::MethodCall* method_call, int timeout_ms,
                    ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        HandleCreateBond(bond, transport, method_call, timeout_ms, cb);
      });

  client_->CreateBond(
      base::BindOnce(&FlossAdapterClientTest::ExpectValidCreateBond,
                     weak_ptr_factory_.GetWeakPtr()),
      bond, transport);
}

TEST_F(FlossAdapterClientTest, CallAdapterMethods) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Method of 0 parameters with no return.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(HasMemberOf(kTestMethod0), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have no parameters.
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with no return value.
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), nullptr);
      });
  client_->CallAdapterMethod(base::BindOnce([](DBusResult<Void> ret) {
                               // Check that there should be no error.
                               EXPECT_TRUE(ret.has_value());
                             }),
                             kTestMethod0);

  // Method of 0 parameters with uint8_t return.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(HasMemberOf(kTestMethod0), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have no parameters.
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with a return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendByte(kFakeU8Return);
        std::move(*cb).Run(response.get(), nullptr);
      });
  client_->CallAdapterMethod(base::BindOnce([](DBusResult<uint8_t> ret) {
                               // Check that return is correctly parsed and
                               // there should be no error.
                               EXPECT_TRUE(ret.has_value());
                               EXPECT_EQ(100, *ret);
                             }),
                             kTestMethod0);

  // Method of 1 parameter with string return.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(HasMemberOf(kTestMethod1), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        uint32_t param1;
        ASSERT_TRUE(msg.PopUint32(&param1));
        EXPECT_EQ(kFakeU32Param, param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with a return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendString(kFakeStrReturn);
        std::move(*cb).Run(response.get(), nullptr);
      });
  client_->CallAdapterMethod(base::BindOnce([](DBusResult<std::string> ret) {
                               // Check that return is correctly parsed and
                               // there should be no error.
                               EXPECT_TRUE(ret.has_value());
                               EXPECT_EQ(kFakeStrReturn, *ret);
                             }),
                             kTestMethod1, kFakeU32Param);

  // Method of 2 parameters with no return.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(HasMemberOf(kTestMethod2), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 2 parameter.
        uint32_t param1;
        std::string param2;
        ASSERT_TRUE(msg.PopUint32(&param1));
        ASSERT_TRUE(msg.PopString(&param2));
        EXPECT_EQ(kFakeU32Param, param1);
        EXPECT_EQ(kFakeStrParam, param2);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with no return value.
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), nullptr);
      });
  std::string str_param(kFakeStrParam);
  client_->CallAdapterMethod(base::BindOnce([](DBusResult<Void> ret) {
                               // Check that there should be no error.
                               EXPECT_TRUE(ret.has_value());
                             }),
                             kTestMethod2, kFakeU32Param, str_param);

  // Method of 0 parameters with invalid return.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(HasMemberOf(kTestMethod0), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have no parameters.
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with a return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(kFakeU8Return);
        std::move(*cb).Run(response.get(), nullptr);
      });
  client_->CallAdapterMethod(base::BindOnce([](DBusResult<uint8_t> ret) {
                               // Check that return cannot be parsed and there
                               // should be an error.
                               EXPECT_FALSE(ret.has_value());
                               EXPECT_EQ(FlossDBusClient::kErrorInvalidReturn,
                                         ret.error().ToString());
                             }),
                             kTestMethod0);
}

TEST_F(FlossAdapterClientTest, GenericMethodGetConnectionState) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Method of 1 parameter with uint32_t return.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kGetConnectionState), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        FlossDeviceId param1;
        ASSERT_TRUE(FlossAdapterClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(FlossDeviceId(
                      {.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
                  param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with a return value.
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(kFakeU32Return);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  base::RunLoop run_loop;
  client_->GetConnectionState(
      base::BindLambdaForTesting([&run_loop](DBusResult<uint32_t> ret) {
        // Check that return is correctly parsed and there should be no
        // error.
        EXPECT_TRUE(ret.has_value());
        EXPECT_EQ(kFakeU32Return, *ret);
        run_loop.Quit();
      }),
      FlossDeviceId({.address = kFakeDeviceAddr, .name = kFakeDeviceName}));
  run_loop.Run();
}

TEST_F(FlossAdapterClientTest,
       GenericMethodConnectAndDisconnectAllEnabledProfiles) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Method of 1 parameter with no return.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kConnectAllEnabledProfiles), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        FlossDeviceId param1;
        ASSERT_TRUE(FlossAdapterClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(FlossDeviceId(
                      {.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
                  param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with no return value.
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kDisconnectAllEnabledProfiles), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        FlossDeviceId param1;
        ASSERT_TRUE(FlossAdapterClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(FlossDeviceId(
                      {.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
                  param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with no return value.
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  base::RunLoop run_loop;
  client_->ConnectAllEnabledProfiles(
      base::BindLambdaForTesting([&run_loop](DBusResult<Void> ret) {
        // Check that there should be no error.
        EXPECT_TRUE(ret.has_value());
        run_loop.Quit();
      }),
      FlossDeviceId({.address = kFakeDeviceAddr, .name = kFakeDeviceName}));
  client_->DisconnectAllEnabledProfiles(
      base::BindLambdaForTesting([&run_loop](DBusResult<Void> ret) {
        // Check that there should be no error.
        EXPECT_TRUE(ret.has_value());
        run_loop.Quit();
      }),
      FlossDeviceId({.address = kFakeDeviceAddr, .name = kFakeDeviceName}));
  run_loop.Run();
}

TEST_F(FlossAdapterClientTest, GenericMethodSetPairingConfirmation) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Method of 2 parameters with no return.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kSetPairingConfirmation), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 2 parameters.
        FlossDeviceId param1;
        bool param2;
        ASSERT_TRUE(FlossAdapterClient::ReadAllDBusParams(&msg, &param1));
        ASSERT_TRUE(msg.PopBool(&param2));
        EXPECT_EQ(FlossDeviceId(
                      {.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
                  param1);
        EXPECT_EQ(kFakeBoolParam, param2);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with no return value.
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  base::RunLoop run_loop;
  client_->SetPairingConfirmation(
      base::BindLambdaForTesting([&run_loop](DBusResult<Void> ret) {
        // Check that there should be no error.
        EXPECT_TRUE(ret.has_value());
        run_loop.Quit();
      }),
      FlossDeviceId({.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
      kFakeBoolParam);
  run_loop.Run();
}

TEST_F(FlossAdapterClientTest, GenericMethodSetPasskey) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Method of 3 parameters with no return.
  EXPECT_CALL(
      *adapter_object_proxy_.get(),
      DoCallMethodWithErrorResponse(HasMemberOf(adapter::kSetPasskey), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 3 parameters.
        FlossDeviceId param1;
        bool param2;
        const uint8_t* param3;
        size_t param3_len;
        ASSERT_TRUE(FlossAdapterClient::ReadAllDBusParams(&msg, &param1));
        ASSERT_TRUE(msg.PopBool(&param2));
        ASSERT_TRUE(msg.PopArrayOfBytes(&param3, &param3_len));
        EXPECT_EQ(FlossDeviceId(
                      {.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
                  param1);
        EXPECT_EQ(kFakeBoolParam, param2);
        EXPECT_EQ(
            std::vector<uint8_t>(kFakeBytes, kFakeBytes + sizeof(kFakeBytes)),
            std::vector<uint8_t>(param3, param3 + param3_len));
        EXPECT_FALSE(msg.HasMoreData());
        // Create a fake response with no return value.
        auto response = ::dbus::Response::CreateEmpty();
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  base::RunLoop run_loop;
  client_->SetPasskey(
      base::BindLambdaForTesting([&run_loop](DBusResult<Void> ret) {
        // Check that there should be no error.
        EXPECT_TRUE(ret.has_value());
        run_loop.Quit();
      }),
      FlossDeviceId({.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
      kFakeBoolParam,
      std::vector<uint8_t>(kFakeBytes, kFakeBytes + sizeof(kFakeBytes)));
  run_loop.Run();
}

TEST_F(FlossAdapterClientTest, GenericMethodGetRemoteUuids) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Method of 1 parameter with UUID response.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kGetRemoteUuids), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        FlossDeviceId param1;
        ASSERT_TRUE(FlossAdapterClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(FlossDeviceId(
                      {.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
                  param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a response with valid UUID. Format is array of UUIDs (array of
        // bytes)
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        dbus::MessageWriter array_writer(nullptr);
        writer.OpenArray("ay", &array_writer);
        array_writer.AppendArrayOfBytes(kFakeUuidByteArray);
        writer.CloseContainer(&array_writer);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  base::RunLoop run_loop;
  client_->GetRemoteUuids(
      base::BindLambdaForTesting(
          [&run_loop](DBusResult<device::BluetoothDevice::UUIDList> ret) {
            // Check that there is no error.
            EXPECT_TRUE(ret.has_value());
            // Check we parse the returned UUID correctly
            device::BluetoothDevice::UUIDList uuid_list = *ret;
            EXPECT_EQ(uuid_list[0], device::BluetoothUUID(kFakeUuidStr));
            run_loop.Quit();
          }),
      FlossDeviceId({.address = kFakeDeviceAddr, .name = kFakeDeviceName}));
  run_loop.Run();
}

TEST_F(FlossAdapterClientTest, GenericMethodGetRemoteType) {
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());

  // Method of 1 parameter with BluetoothDeviceType response.
  EXPECT_CALL(
      *adapter_object_proxy_.get(),
      DoCallMethodWithErrorResponse(HasMemberOf(adapter::kGetRemoteType), _, _))
      .WillOnce([](::dbus::MethodCall* method_call, int timeout_ms,
                   ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have 1 parameter.
        FlossDeviceId param1;
        ASSERT_TRUE(FlossAdapterClient::ReadAllDBusParams(&msg, &param1));
        EXPECT_EQ(FlossDeviceId(
                      {.address = kFakeDeviceAddr, .name = kFakeDeviceName}),
                  param1);
        EXPECT_FALSE(msg.HasMoreData());
        // Create a response with valid BluetoothDeviceType
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendUint32(static_cast<uint32_t>(kFakeType));
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  base::RunLoop run_loop;
  client_->GetRemoteType(
      base::BindLambdaForTesting(
          [&run_loop](
              DBusResult<floss::FlossAdapterClient::BluetoothDeviceType> ret) {
            // Check that there is no error.
            EXPECT_TRUE(ret.has_value());
            // Check we parse the returned type correctly
            EXPECT_EQ(*ret, kFakeType);
            run_loop.Quit();
          }),
      FlossDeviceId({.address = kFakeDeviceAddr, .name = kFakeDeviceName}));
  run_loop.Run();
}

TEST_F(FlossAdapterClientTest, OnAdapterPropertyChanged) {
  TestAdapterObserver test_observer(client_.get());
  client_->Init(bus_.get(), kAdapterInterface, adapter_index_, GetCurrVersion(),
                base::DoNothing());
  EXPECT_EQ(test_observer.found_device_count_, 0);

  // Method of no parameters with vector of FlossDeviceId response.
  EXPECT_CALL(*adapter_object_proxy_.get(),
              DoCallMethodWithErrorResponse(
                  HasMemberOf(adapter::kGetBondedDevices), _, _))
      .WillOnce([this](::dbus::MethodCall* method_call, int timeout_ms,
                       ::dbus::ObjectProxy::ResponseOrErrorCallback* cb) {
        dbus::MessageReader msg(method_call);
        // D-Bus method call should have no parameters.
        EXPECT_FALSE(msg.HasMoreData());
        // Create a response with valid array of FlossDeviceIds
        FlossDeviceId device_id = {.address = "66:55:44:33:22:11",
                                   .name = "First"};
        auto response = ::dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        dbus::MessageWriter array(nullptr);
        writer.OpenArray("a{sv}", &array);
        this->EncodeFlossDeviceId(&array, device_id,
                                  /*include_required_keys=*/true,
                                  /*include_extra_keys=*/false);
        writer.CloseContainer(&array);
        std::move(*cb).Run(response.get(), /*err=*/nullptr);
      });
  SendAdapterPropertyChangedCallback(
      /*error=*/false,
      FlossAdapterClient::BtPropertyType::kAdapterBondedDevices,
      base::BindOnce(&FlossAdapterClientTest::ExpectNormalResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_EQ(test_observer.found_device_count_, 1);
}

}  // namespace floss
