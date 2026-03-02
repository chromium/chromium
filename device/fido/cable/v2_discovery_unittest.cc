// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_discovery.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "crypto/random.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/cable/cable_mock_bluetooth_adapter.h"
#include "device/fido/cable/pairing.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_test_util.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "device/fido/public/cable_discovery_data.h"
#include "device/fido/public/fido_constants.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/floss/floss_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/util.h"
#endif

using ::testing::_;
using ::testing::NiceMock;

namespace device::cablev2 {
namespace {

ACTION_P(ReturnFromAsyncCall, closure) {
  closure.Run();
}

class MockTunnelServer : public network::TestNetworkContext {
 public:
  // Too many arguments for gmock. :(
  void CreateWebSocket(
      const GURL& url,
      const std::vector<std::string>& requested_protocols,
      net::StorageAccessApiStatus storage_access_api_status,
      const net::IsolationInfo& isolation_info,
      std::vector<network::mojom::HttpHeaderPtr> additional_headers,
      const network::OriginatingProcessId& process_id,
      const url::Origin& origin,
      network::mojom::ClientSecurityStatePtr client_security_state,
      uint32_t options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
          auth_handler,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client,
      const std::optional<base::UnguessableToken>& throttling_profile_id)
      override {
    create_called_ = true;
  }

  bool create_called_ = false;
};

}  // namespace

class CableV2DiscoveryTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_tunnel_server_ = std::make_unique<MockTunnelServer>();

    discovery_ = std::make_unique<Discovery>(
        device::FidoRequestType::kGetAssertion,
        base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
          return mock_tunnel_server_.get();
        }),
        qr_generator_key_, /*contact_device_stream=*/nullptr,
        /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
        /*pairing_callback=*/std::nullopt,
        /*invalidated_pairing_callback=*/std::nullopt,
        /*event_callback=*/std::nullopt, /*must_support_ctap=*/true);
  }

  void TearDown() override {
    discovery_.reset();
    mock_tunnel_server_.reset();
  }

  Discovery* discovery() { return discovery_.get(); }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  MockTunnelServer& mock_tunnel_server() { return *mock_tunnel_server_.get(); }

  std::array<uint8_t, kAdvertSize> GetV2Advert() {
    CableEidArray plaintext_eid;
    device::cablev2::eid::Components components;
    components.tunnel_server_domain = kTunnelServer;
    components.routing_id = {0};
    crypto::RandBytes(components.nonce);

    plaintext_eid = device::cablev2::eid::FromComponents(components);

    return eid::Encrypt(
        plaintext_eid,
        Derive<kEIDKeySize>(zero_qr_secret_, {},
                            device::cablev2::DerivedValueType::kEIDKey));
  }

  std::array<uint8_t, kAdvertSize> GetNonMatchingV2Advert() {
    CableEidArray plaintext_eid;
    device::cablev2::eid::Components components;
    components.tunnel_server_domain = kTunnelServer;
    components.routing_id = {0};
    crypto::RandBytes(components.nonce);

    plaintext_eid = device::cablev2::eid::FromComponents(components);

    return eid::Encrypt(
        plaintext_eid,
        Derive<kEIDKeySize>(one_qr_secret_, {},
                            device::cablev2::DerivedValueType::kEIDKey));
  }

 private:
  std::unique_ptr<Discovery> discovery_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockTunnelServer> mock_tunnel_server_;

  const std::array<uint8_t, kQRKeySize> qr_generator_key_ = {0};
  const std::array<uint8_t, kQRSecretSize> zero_qr_secret_ = {0};
  const std::array<uint8_t, kQRSecretSize> one_qr_secret_ = {1};
};

// Tests discovery without a BLE adapter.
TEST_F(CableV2DiscoveryTest, TestDiscoveryFails) {
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(
      mock_observer,
      DiscoveryStarted(discovery(), false, std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _)).Times(0);
  discovery()->set_observer(&mock_observer);

  auto mock_adapter = CableMockBluetoothAdapter::MakeNotPresent();
  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  discovery()->Start();
  task_environment().FastForwardUntilNoTasksRemain();
}

// Tests discovery with a powered-off BLE adapter.  Not calling
// DiscoveryStarted() in the case of a present-but-unpowered adapter leads to a
// deadlock between the discovery and the UI (see crbug.com/1018416).
TEST_F(CableV2DiscoveryTest, TestDiscoveryStartedWithUnpoweredAdapter) {
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(
      mock_observer,
      DiscoveryStarted(discovery(), true, std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _)).Times(0);
  discovery()->set_observer(&mock_observer);

  auto mock_adapter = CableMockBluetoothAdapter::MakePoweredOff();
  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  discovery()->Start();
  task_environment().FastForwardUntilNoTasksRemain();
}

// Tests regular successful discovery flow for device.
TEST_F(CableV2DiscoveryTest, TestDiscoveryFindsNewDevice) {
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(
      mock_observer,
      DiscoveryStarted(discovery(), true, std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  discovery()->set_observer(&mock_observer);

  auto mock_adapter = CableMockBluetoothAdapter::MakePoweredOn();
  mock_adapter->ExpectDiscoveryWithScanCallback(GetV2Advert());

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  discovery()->Start();
  task_environment().FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(mock_tunnel_server().create_called_);
}

#if BUILDFLAG(IS_MAC)

// Tests that the discovery will not attempt to call bluetooth functions like
// IsPowered() if the build is signed and the OS reports an undetermined
// permission status.
TEST_F(CableV2DiscoveryTest, TestDiscoveryDoesNotUseBluetoothIfUnauthorized) {
  fido::mac::ScopedProcessIsSignedOverride scoped_process_is_signed_override(
      fido::mac::CodeSigningState::kSigned);
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(
      mock_observer,
      DiscoveryStarted(discovery(), true, std::vector<FidoAuthenticator*>()));
  discovery()->set_observer(&mock_observer);

  auto mock_adapter =
      CableMockBluetoothAdapter::MakeWithUndeterminedPermission();
  EXPECT_CALL(*mock_adapter, IsPowered()).Times(0);
  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  discovery()->Start();
  task_environment().FastForwardUntilNoTasksRemain();
}

// Tests that the discovery will assume bluetooth permission is granted if the
// build is not signed.
TEST_F(CableV2DiscoveryTest,
       TestDiscoveryAssumesBluetoothAuthorizedIfUnsigned) {
  fido::mac::ScopedProcessIsSignedOverride scoped_process_is_signed_override(
      fido::mac::CodeSigningState::kNotSigned);
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(
      mock_observer,
      DiscoveryStarted(discovery(), true, std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  discovery()->set_observer(&mock_observer);

  auto mock_adapter =
      CableMockBluetoothAdapter::MakeWithUndeterminedPermission();
  EXPECT_CALL(*mock_adapter, IsPowered())
      .WillRepeatedly(::testing::Return(true));
  mock_adapter->ExpectDiscoveryWithScanCallback(GetV2Advert());

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  discovery()->Start();
  task_environment().FastForwardUntilNoTasksRemain();
}

#endif  // BUILDFLAG(IS_MAC)

// Tests a scenario where upon broadcasting advertisement and scanning, client
// discovers a device with an incorrect authenticator EID. Observer::AddDevice()
// must not be called.
TEST_F(CableV2DiscoveryTest, TestDiscoveryFindsIncorrectDevice) {
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _)).Times(0);
  EXPECT_CALL(mock_observer,
              DiscoveryStarted(discovery(), true, testing::IsEmpty()));
  discovery()->set_observer(&mock_observer);

  auto mock_adapter = CableMockBluetoothAdapter::MakePoweredOn();
  mock_adapter->ExpectDiscoveryWithScanCallback(GetNonMatchingV2Advert());

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  discovery()->Start();
  task_environment().FastForwardUntilNoTasksRemain();
}

// Tests that cable discovery resumes after Bluetooth adapter is powered on.
TEST_F(CableV2DiscoveryTest, TestResumeDiscoveryAfterPoweredOn) {
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(
      mock_observer,
      DiscoveryStarted(discovery(), true, std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  discovery()->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(::testing::Return(true));

  // After BluetoothAdapter is powered on, we expect that Cable discovery starts
  // again.
  mock_adapter->ExpectDiscoveryWithScanCallback(GetV2Advert());

  // Wait until error callback for SetPowered() is invoked. Then, simulate
  // Bluetooth adapter power change by invoking
  // MockBluetoothAdapter::NotifyAdapterPoweredChanged().
  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(*mock_adapter, IsPowered())
        .WillRepeatedly(::testing::DoAll(ReturnFromAsyncCall(quit),
                                         ::testing::Return(false)));

    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
    discovery()->Start();
    run_loop.Run();
  }

  mock_adapter->NotifyAdapterPoweredChanged(true);
  task_environment().FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(mock_tunnel_server().create_called_);
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests regular successful discovery flow for Cable device on Floss.
TEST_F(CableV2DiscoveryTest, TestDiscoveryFindsNewDeviceFloss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(floss::features::kFlossEnabled);

  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(
      mock_observer,
      DiscoveryStarted(discovery(), true, std::vector<FidoAuthenticator*>()));
  EXPECT_CALL(mock_observer, AuthenticatorAdded(_, _));
  discovery()->set_observer(&mock_observer);

  auto mock_adapter = CableMockBluetoothAdapter::MakePoweredOn();
  mock_adapter->ExpectLEScan(GetV2Advert());

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  discovery()->Start();
  task_environment().FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(mock_tunnel_server().create_called_);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace device::cablev2
