// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_discovery.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/ble/fido_ble_device.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/cable/fido_cable_handshake_handler.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Sequence;

namespace device {

namespace {

constexpr auto kTestCableVersion = CableDiscoveryData::Version::V1;
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
constexpr auto kTestCableVersionNumber = 1;
#endif

// Constants required for discovering and constructing a Cable device that
// are given by the relying party via an extension.
constexpr CableEidArray kClientEid = {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                       0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13,
                                       0x14, 0x15}};

constexpr char kUuidFormattedClientEid[] =
    "00010203-0405-0607-0809-101112131415";

constexpr CableEidArray kAuthenticatorEid = {
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
     0x01, 0x01, 0x01, 0x01}};

constexpr CableEidArray kInvalidAuthenticatorEid = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00}};

constexpr CableSessionPreKeyArray kTestSessionPreKey = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

// TODO(https://crbug.com/837088): Add support for multiple EIDs on Windows.
#if !defined(OS_WIN)
constexpr CableEidArray kSecondaryClientEid = {
    {0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
     0x03, 0x02, 0x01, 0x00}};

constexpr char kUuidFormattedSecondaryClientEid[] =
    "15141312-1110-0908-0706-050403020100";

constexpr CableEidArray kSecondaryAuthenticatorEid = {
    {0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
     0xee, 0xee, 0xee, 0xee}};

constexpr CableSessionPreKeyArray kSecondarySessionPreKey = {
    {0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
     0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
     0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd}};
#endif  // !defined(OS_WIN)

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";

constexpr char kTestBleDeviceName[] = "test_cable_device";

std::unique_ptr<MockBluetoothDevice> CreateTestBluetoothDevice() {
  return std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
      nullptr /* adapter */, 0 /* bluetooth_class */, kTestBleDeviceName,
      kTestBleDeviceAddress, true /* paired */, true /* connected */);
}

ACTION_P(ReturnFromAsyncCall, closure) {
  closure.Run();
}

// Matcher to compare the content of advertisement data received from the
// client.
MATCHER_P2(IsAdvertisementContent,
           expected_client_eid,
           expected_uuid_formatted_client_eid,
           "") {
#if defined(OS_MACOSX)
  const auto uuid_list = arg->service_uuids();
  return std::any_of(uuid_list->begin(), uuid_list->end(),
                     [this](const auto& uuid) {
                       return uuid == expected_uuid_formatted_client_eid;
                     });

#elif defined(OS_WIN)
  const auto manufacturer_data = arg->manufacturer_data();
  const auto manufacturer_data_value = manufacturer_data->find(0x00E0);

  if (manufacturer_data_value == manufacturer_data->end())
    return false;

  const auto& manufacturer_data_payload = manufacturer_data_value->second;
  return manufacturer_data_payload.size() >= 4u &&
         manufacturer_data_payload[0] == manufacturer_data_payload.size() - 1 &&
         manufacturer_data_payload[1] == 0x15 &&  // Manufacturer Data Type
         manufacturer_data_payload[2] == 0x20 &&  // Cable Flags
         manufacturer_data_payload[3] == kTestCableVersionNumber &&
         std::equal(manufacturer_data_payload.begin() + 4,
                    manufacturer_data_payload.end(),
                    expected_client_eid.begin(), expected_client_eid.end());

#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  const auto service_data = arg->service_data();
  const auto service_data_with_uuid =
      service_data->find(kCableAdvertisementUUID128);

  if (service_data_with_uuid == service_data->end())
    return false;

  const auto& service_data_value = service_data_with_uuid->second;
  return (service_data_value[0] >> 5 & 1) &&
         service_data_value[1] == kTestCableVersionNumber &&
         service_data_value.size() == 18u &&
         std::equal(service_data_value.begin() + 2, service_data_value.end(),
                    expected_client_eid.begin(), expected_client_eid.end());

#endif

  return true;
}

class CableMockBluetoothAdvertisement : public BluetoothAdvertisement {
 public:
  MOCK_METHOD2(Unregister,
               void(const SuccessCallback& success_callback,
                    const ErrorCallback& error_callback));

 private:
  ~CableMockBluetoothAdvertisement() override = default;
};

// Mock BLE adapter that abstracts out authenticator logic with the following
// logic:
//  - Responds to BluetoothAdapter::RegisterAdvertisement() by always invoking
//    success callback.
//  - Responds to BluetoothAdapter::StartDiscoverySessionWithFilter() by
//    invoking BluetoothAdapter::Observer::DeviceAdded() on a test bluetooth
//    device that includes service data containing authenticator EID.
class CableMockAdapter : public MockBluetoothAdapter {
 public:
  MOCK_METHOD3(RegisterAdvertisement,
               void(std::unique_ptr<BluetoothAdvertisement::Data>,
                    const CreateAdvertisementCallback&,
                    const AdvertisementErrorCallback&));

  void AddNewTestBluetoothDevice(
      base::span<const uint8_t, kCableEphemeralIdSize> authenticator_eid) {
    auto mock_device = CreateTestBluetoothDevice();

    std::vector<uint8_t> service_data(18);
    service_data[0] = 1 << 5;
    std::copy(authenticator_eid.begin(), authenticator_eid.end(),
              service_data.begin() + 2);
    BluetoothDevice::ServiceDataMap service_data_map;
    service_data_map.emplace(kCableAdvertisementUUID128,
                             std::move(service_data));

    mock_device->UpdateAdvertisementData(
        1 /* rssi */, base::nullopt /* flags */, BluetoothDevice::UUIDList(),
        base::nullopt /* tx_power */, std::move(service_data_map),
        BluetoothDevice::ManufacturerDataMap());

    auto* mock_device_ptr = mock_device.get();
    AddMockDevice(std::move(mock_device));

    for (auto& observer : GetObservers())
      observer.DeviceAdded(this, mock_device_ptr);
  }

  void AddNewTestAppleBluetoothDevice(
      base::span<const uint8_t, kCableEphemeralIdSize> authenticator_eid) {
    auto mock_device = CreateTestBluetoothDevice();
    // Apple doesn't allow advertising service data, so we advertise a 16 bit
    // UUID plus the EID converted into 128 bit UUID.
    mock_device->AddUUID(BluetoothUUID("fde2"));
    mock_device->AddUUID(BluetoothUUID(
        fido_parsing_utils::ConvertBytesToUuid(authenticator_eid)));

    auto* mock_device_ptr = mock_device.get();
    AddMockDevice(std::move(mock_device));

    for (auto& observer : GetObservers())
      observer.DeviceAdded(this, mock_device_ptr);
  }

  void ExpectRegisterAdvertisementWithResponse(
      bool simulate_success,
      base::span<const uint8_t> expected_client_eid,
      base::StringPiece expected_uuid_formatted_client_eid,
      Sequence sequence = Sequence(),
      scoped_refptr<CableMockBluetoothAdvertisement> advertisement = nullptr) {
    if (!advertisement) {
      advertisement = base::MakeRefCounted<CableMockBluetoothAdvertisement>();
      EXPECT_CALL(*advertisement, Unregister(_, _))
          .WillRepeatedly(::testing::WithArg<0>(
              [](const auto& callback) { callback.Run(); }));
    }

    EXPECT_CALL(*this,
                RegisterAdvertisement(
                    IsAdvertisementContent(expected_client_eid,
                                           expected_uuid_formatted_client_eid),
                    _, _))
        .InSequence(sequence)
        .WillOnce(::testing::WithArgs<1, 2>(
            [simulate_success, advertisement](const auto& success_callback,
                                              const auto& failure_callback) {
              simulate_success
                  ? success_callback.Run(advertisement)
                  : failure_callback.Run(BluetoothAdvertisement::ErrorCode::
                                             INVALID_ADVERTISEMENT_ERROR_CODE);
            }));
  }

  void ExpectSuccessCallbackToIsPowered() {
    EXPECT_CALL(*this, IsPresent()).WillOnce(::testing::Return(true));
    EXPECT_CALL(*this, IsPowered()).WillOnce(::testing::Return(true));
  }

  void ExpectDiscoveryWithScanCallback() {
    EXPECT_CALL(*this, StartScanWithFilter_(_, _))
        .WillOnce(::testing::WithArg<1>([](auto& callback) {
          std::move(callback).Run(
              false, device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
        }));
  }

  void ExpectDiscoveryWithScanCallback(
      base::span<const uint8_t, kCableEphemeralIdSize> eid,
      bool is_apple_device = false) {
    EXPECT_CALL(*this, StartScanWithFilter_(_, _))
        .WillOnce(
            ::testing::WithArg<1>([this, eid, is_apple_device](auto& callback) {
              std::move(callback).Run(
                  false, device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
              if (is_apple_device) {
                AddNewTestAppleBluetoothDevice(eid);
              } else {
                AddNewTestBluetoothDevice(eid);
              }
            }));
  }

 protected:
  ~CableMockAdapter() override = default;
};

class FakeHandshakeHandler : public FidoCableV1HandshakeHandler {
 public:
  FakeHandshakeHandler(FidoCableDevice* device,
                       base::span<const uint8_t, 8> nonce,
                       base::span<const uint8_t, 32> session_pre_key)
      : FidoCableV1HandshakeHandler(device, nonce, session_pre_key) {}
  ~FakeHandshakeHandler() override = default;

  void InitiateCableHandshake(FidoDevice::DeviceCallback callback) override {
    std::move(callback).Run(std::vector<uint8_t>());
  }

  bool ValidateAuthenticatorHandshakeMessage(
      base::span<const uint8_t> response) override {
    return true;
  }
};

// Fake discovery that encapsulates exactly the same behavior as
// FidoCableDiscovery except that it uses FakeHandshakeHandler instead of
// FidoHandshakeHandler to conduct handshake with the authenticator.
class FakeFidoCableDiscovery : public FidoCableDiscovery {
 public:
  explicit FakeFidoCableDiscovery(
      std::vector<CableDiscoveryData> discovery_data)
      : FidoCableDiscovery(std::move(discovery_data),
                           BogusQRGeneratorKey(),
                           /*pairing_callback=*/base::nullopt) {}
  ~FakeFidoCableDiscovery() override = default;

 private:
  base::Optional<std::unique_ptr<FidoCableHandshakeHandler>>
  CreateHandshakeHandler(FidoCableDevice* device,
                         const CableDiscoveryData& discovery_data,
                         const CableNonce& nonce,
                         const CableEidArray& eid) override {
    return std::make_unique<FakeHandshakeHandler>(
        device, nonce, discovery_data.v1->session_pre_key);
  }

  static std::array<uint8_t, 32> BogusQRGeneratorKey() {
    std::array<uint8_t, 32> ret;
    memset(ret.data(), 0, ret.size());
    return ret;
  }
};

}  // namespace

class FidoCableDiscoveryTest : public ::testing::Test {
 public:
  std::unique_ptr<FidoCableDiscovery> CreateDiscovery() {
    std::vector<CableDiscoveryData> discovery_data;
    discovery_data.emplace_back(kTestCableVersion, kClientEid,
                                kAuthenticatorEid, kTestSessionPreKey);
    return std::make_unique<FakeFidoCableDiscovery>(std::move(discovery_data));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests discovery without a BLE adapter.
TEST_F(FidoCableDiscoveryTest, TestDiscoveryFails) {
  auto cable_discovery = CreateDiscovery();
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer,
              DiscoveryStarted(cable_discovery.get(), false,
                               std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _)).Times(0);
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent()).WillOnce(::testing::Return(false));

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests discovery with a powered-off BLE adapter.  Not calling
// DiscoveryStarted() in the case of a present-but-unpowered adapter leads to a
// deadlock between the discovery and the UI (see crbug.com/1018416).
TEST_F(FidoCableDiscoveryTest, TestDiscoveryStartedWithUnpoweredAdapter) {
  auto cable_discovery = CreateDiscovery();
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer,
              DiscoveryStarted(cable_discovery.get(), true,
                               std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _)).Times(0);
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*mock_adapter, IsPowered()).WillOnce(::testing::Return(false));

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests regular successful discovery flow for Cable device.
TEST_F(FidoCableDiscoveryTest, TestDiscoveryFindsNewDevice) {
  auto cable_discovery = CreateDiscovery();
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer,
              DiscoveryStarted(cable_discovery.get(), true,
                               std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  mock_adapter->ExpectSuccessCallbackToIsPowered();
  mock_adapter->ExpectDiscoveryWithScanCallback(kAuthenticatorEid);
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      true /* simulate_success */, kClientEid, kUuidFormattedClientEid);

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests successful discovery flow for Apple Cable device.
TEST_F(FidoCableDiscoveryTest, TestDiscoveryFindsNewAppleDevice) {
  auto cable_discovery = CreateDiscovery();
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer,
              DiscoveryStarted(cable_discovery.get(), true,
                               std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  mock_adapter->ExpectSuccessCallbackToIsPowered();
  mock_adapter->ExpectDiscoveryWithScanCallback(kAuthenticatorEid, true);
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      true /* simulate_success */, kClientEid, kUuidFormattedClientEid);

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests a scenario where upon broadcasting advertisement and scanning, client
// discovers a device with an incorrect authenticator EID. Observer::AddDevice()
// must not be called.
TEST_F(FidoCableDiscoveryTest, TestDiscoveryFindsIncorrectDevice) {
  auto cable_discovery = CreateDiscovery();
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _)).Times(0);
  EXPECT_CALL(mock_observer, DiscoveryStarted(cable_discovery.get(), true,
                                              testing::IsEmpty()));
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  mock_adapter->ExpectSuccessCallbackToIsPowered();
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      true /* simulate_success */, kClientEid, kUuidFormattedClientEid);
  mock_adapter->ExpectDiscoveryWithScanCallback(kInvalidAuthenticatorEid);

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Windows currently does not support multiple EIDs, so the following tests are
// not applicable.
// TODO(https://crbug.com/837088): Support multiple EIDs on Windows and enable
// these tests.
#if !defined(OS_WIN)
// Tests Cable discovery flow when multiple(2) sets of client/authenticator EIDs
// are passed on from the relying party. We should expect 2 invocations of
// BluetoothAdapter::RegisterAdvertisement().
TEST_F(FidoCableDiscoveryTest, TestDiscoveryWithMultipleEids) {
  std::vector<CableDiscoveryData> discovery_data;
  discovery_data.emplace_back(kTestCableVersion, kClientEid, kAuthenticatorEid,
                              kTestSessionPreKey);
  discovery_data.emplace_back(kTestCableVersion, kSecondaryClientEid,
                              kSecondaryAuthenticatorEid,
                              kSecondarySessionPreKey);
  auto cable_discovery =
      std::make_unique<FakeFidoCableDiscovery>(std::move(discovery_data));
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  mock_adapter->ExpectSuccessCallbackToIsPowered();
  mock_adapter->ExpectDiscoveryWithScanCallback(kAuthenticatorEid);

  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer,
              DiscoveryStarted(cable_discovery.get(), true,
                               std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  cable_discovery->set_observer(&mock_observer);

  Sequence sequence;
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      true /* simulate_success */, kClientEid, kUuidFormattedClientEid,
      sequence);
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      true /* simulate_success */, kSecondaryClientEid,
      kUuidFormattedSecondaryClientEid, sequence);

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Tests a scenario where only one of the two client EID's are advertised
// successfully. Since at least one advertisement are successfully processed,
// scanning process should be invoked.
TEST_F(FidoCableDiscoveryTest, TestDiscoveryWithPartialAdvertisementSuccess) {
  std::vector<CableDiscoveryData> discovery_data;
  discovery_data.emplace_back(kTestCableVersion, kClientEid, kAuthenticatorEid,
                              kTestSessionPreKey);
  discovery_data.emplace_back(kTestCableVersion, kSecondaryClientEid,
                              kSecondaryAuthenticatorEid,
                              kSecondarySessionPreKey);
  auto cable_discovery =
      std::make_unique<FakeFidoCableDiscovery>(std::move(discovery_data));
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer,
              DiscoveryStarted(cable_discovery.get(), true,
                               std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  mock_adapter->ExpectSuccessCallbackToIsPowered();
  Sequence sequence;
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      true /* simulate_success */, kClientEid, kUuidFormattedClientEid,
      sequence);
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      false /* simulate_success */, kSecondaryClientEid,
      kUuidFormattedSecondaryClientEid, sequence);
  mock_adapter->ExpectDiscoveryWithScanCallback(kAuthenticatorEid);

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

// Test the scenario when all advertisement for client EID's fails.
TEST_F(FidoCableDiscoveryTest, TestDiscoveryWithAdvertisementFailures) {
  std::vector<CableDiscoveryData> discovery_data;
  discovery_data.emplace_back(kTestCableVersion, kClientEid, kAuthenticatorEid,
                              kTestSessionPreKey);
  discovery_data.emplace_back(kTestCableVersion, kSecondaryClientEid,
                              kSecondaryAuthenticatorEid,
                              kSecondarySessionPreKey);
  auto cable_discovery =
      std::make_unique<FakeFidoCableDiscovery>(std::move(discovery_data));

  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _)).Times(0);
  EXPECT_CALL(mock_observer, DiscoveryStarted(cable_discovery.get(), true,
                                              testing::IsEmpty()));
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  Sequence sequence;
  mock_adapter->ExpectSuccessCallbackToIsPowered();
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      false /* simulate_success */, kClientEid, kUuidFormattedClientEid,
      sequence);
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      false /* simulate_success */, kSecondaryClientEid,
      kUuidFormattedSecondaryClientEid, sequence);
  mock_adapter->ExpectDiscoveryWithScanCallback();

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(cable_discovery->advertisements_.empty());
}
#endif  // !defined(OS_WIN)

TEST_F(FidoCableDiscoveryTest, TestUnregisterAdvertisementUponDestruction) {
  auto cable_discovery = CreateDiscovery();
  CableMockBluetoothAdvertisement* advertisement =
      new CableMockBluetoothAdvertisement();
  EXPECT_CALL(*advertisement, Unregister(_, _)).Times(1);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  mock_adapter->ExpectSuccessCallbackToIsPowered();
  mock_adapter->ExpectDiscoveryWithScanCallback();
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      true /* simulate_success */, kClientEid, kUuidFormattedClientEid,
      Sequence(), base::WrapRefCounted(advertisement));

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(1u, cable_discovery->advertisements_.size());
  cable_discovery.reset();
}

// Tests that cable discovery resumes after Bluetooth adapter is powered on.
TEST_F(FidoCableDiscoveryTest, TestResumeDiscoveryAfterPoweredOn) {
  auto cable_discovery = CreateDiscovery();
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer,
              DiscoveryStarted(cable_discovery.get(), true,
                               std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent()).WillOnce(::testing::Return(true));

  // After BluetoothAdapter is powered on, we expect that Cable discovery starts
  // again.
  mock_adapter->ExpectDiscoveryWithScanCallback(kAuthenticatorEid);
  mock_adapter->ExpectRegisterAdvertisementWithResponse(
      true /* simulate_success */, kClientEid, kUuidFormattedClientEid);

  // Wait until error callback for SetPowered() is invoked. Then, simulate
  // Bluetooth adapter power change by invoking
  // MockBluetoothAdapter::NotifyAdapterPoweredChanged().
  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(*mock_adapter, IsPowered)
        .WillOnce(::testing::DoAll(ReturnFromAsyncCall(quit),
                                   ::testing::Return(false)));

    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
    cable_discovery->Start();
    run_loop.Run();
  }

  mock_adapter->NotifyAdapterPoweredChanged(true);
  task_environment_.FastForwardUntilNoTasksRemain();
}

}  // namespace device
