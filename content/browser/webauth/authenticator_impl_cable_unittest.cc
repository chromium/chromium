// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/browser/webauth/authenticator_impl_unittest_test_base.h"
#include "content/browser/webauth/default_authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "crypto/hash.h"
#include "crypto/hmac.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/cable/cable_mock_bluetooth_adapter.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/cable/pairing.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/cable/v2_test_util.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/floss/floss_features.h"
#endif

namespace content {
namespace {

using device::VirtualCtap2Device;
using device::VirtualFidoDevice;
using device::cablev2::Event;

class AuthenticatorCableV2Test : public AuthenticatorImplRequestDelegateTest {
 public:
  void SetUp() override {
    AuthenticatorImplTest::SetUp();

    NavigateAndCommit(GURL(kTestOrigin1));
    ResetNetworkService();

    old_client_ = SetBrowserClientForTesting(&browser_client_);

    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    bssl::UniquePtr<EC_KEY> peer_identity(EC_KEY_derive_from_secret(
        p256.get(), zero_seed_.data(), zero_seed_.size()));
    CHECK_EQ(sizeof(peer_identity_x962_),
             EC_POINT_point2oct(
                 p256.get(), EC_KEY_get0_public_key(peer_identity.get()),
                 POINT_CONVERSION_UNCOMPRESSED, peer_identity_x962_,
                 sizeof(peer_identity_x962_), /*ctx=*/nullptr));

    // These tests use a more specialized adapter than is used in the base
    // class.
    mock_bluetooth_adapter_ =
        device::cablev2::CableMockBluetoothAdapter::MakePoweredOn();
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);
  }

  void TearDown() override {
    // Ensure that all pending caBLE connections have timed out and closed.
    task_environment()->FastForwardBy(base::Minutes(10));

    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();

    // All `EstablishedConnection` instances should have been destroyed.
    CHECK_EQ(device::cablev2::FidoTunnelDevice::
                 GetNumEstablishedConnectionInstancesForTesting(),
             0);
  }

  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
  GetPairingCallback() {
    return base::BindRepeating(&AuthenticatorCableV2Test::OnNewPairing,
                               base::Unretained(this));
  }

  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
  GetInvalidatedPairingCallback() {
    return base::BindRepeating(&AuthenticatorCableV2Test::OnInvalidatedPairing,
                               base::Unretained(this));
  }

  base::RepeatingCallback<void(Event)> GetEventCallback() {
    return base::BindRepeating(&AuthenticatorCableV2Test::OnCableEvent,
                               base::Unretained(this));
  }

  void EnableConnectionSignalAtTunnelServer() {
    // Recreate the tunnel server so that it supports the connection signal.
    network_context_ = device::cablev2::NewMockTunnelServer(
        base::BindRepeating(&AuthenticatorCableV2Test::OnContact,
                            base::Unretained(this)),
        /*supports_connect_signal=*/true);
  }

 protected:
  class DiscoveryFactory : public device::FidoDiscoveryFactory {
   public:
    explicit DiscoveryFactory(
        std::unique_ptr<device::cablev2::Discovery> discovery)
        : discovery_(std::move(discovery)) {}

    std::vector<std::unique_ptr<device::FidoDiscoveryBase>> Create(
        device::FidoTransportProtocol transport) override {
      if (transport != device::FidoTransportProtocol::kHybrid || !discovery_) {
        return {};
      }

      return SingleDiscovery(std::move(discovery_));
    }

   private:
    std::unique_ptr<device::cablev2::Discovery> discovery_;
  };

  class TestAuthenticationDelegate : public WebAuthenticationDelegate {
   public:
    bool SupportsResidentKeys(RenderFrameHost*) override { return true; }

    bool IsFocused(WebContents* web_contents) override { return true; }
  };

  class ContactWhenReadyAuthenticatorRequestDelegate
      : public DefaultAuthenticatorRequestClientDelegate {
   public:
    explicit ContactWhenReadyAuthenticatorRequestDelegate(
        base::RepeatingClosure callback)
        : callback_(callback) {}
    ~ContactWhenReadyAuthenticatorRequestDelegate() override = default;

    void OnTransportAvailabilityEnumerated(
        device::FidoRequestHandlerBase::TransportAvailabilityInfo) override {
      callback_.Run();
    }

   private:
    base::RepeatingClosure callback_;
  };

  class ContactWhenReadyContentBrowserClient : public ContentBrowserClient {
   public:
    explicit ContactWhenReadyContentBrowserClient(
        base::RepeatingClosure callback)
        : callback_(callback) {}

    std::unique_ptr<AuthenticatorRequestClientDelegate>
    GetWebAuthenticationRequestDelegate(
        RenderFrameHost* render_frame_host) override {
      return std::make_unique<ContactWhenReadyAuthenticatorRequestDelegate>(
          callback_);
    }

    WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
      return &authentication_delegate_;
    }

   private:
    base::RepeatingClosure callback_;
    TestAuthenticationDelegate authentication_delegate_;
  };

  // MaybeContactPhones is called when OnTransportAvailabilityEnumerated is
  // called by the request handler.
  void MaybeContactPhones() {
    if (maybe_contact_phones_callback_) {
      std::move(maybe_contact_phones_callback_).Run();
    }
  }

  void OnContact(
      base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
      base::span<const uint8_t, device::cablev2::kPairingIDSize> pairing_id,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      const std::string& request_type_hint) {
    std::move(contact_callback_)
        .Run(tunnel_id, pairing_id, client_nonce, request_type_hint);
  }

  void OnNewPairing(std::unique_ptr<device::cablev2::Pairing> pairing) {
    pairings_.emplace_back(std::move(pairing));
  }

  void OnInvalidatedPairing(
      std::unique_ptr<device::cablev2::Pairing> disabled_pairing) {
    pairings_.erase(std::ranges::find_if(
        pairings_, [&disabled_pairing](const auto& pairing) {
          return device::cablev2::Pairing::EqualPublicKeys(pairing,
                                                           disabled_pairing);
        }));
  }

  void OnCableEvent(Event event) { events_.push_back(event); }

  void MaybeExpectDiscoveryWithScanCallback() {
#if BUILDFLAG(IS_CHROMEOS)
    if (!floss::features::IsFlossEnabled()) {
      mock_bluetooth_adapter_->ExpectDiscoveryWithScanCallback();
    }
#else
    mock_bluetooth_adapter_->ExpectDiscoveryWithScanCallback();
#endif
  }

  void DoPairingConnection() {
    // First do unpaired exchange to get pairing data.
    auto discovery = std::make_unique<device::cablev2::Discovery>(
        device::FidoRequestType::kGetAssertion,
        base::BindLambdaForTesting([&]() { return network_context_.get(); }),
        qr_generator_key_,
        /*contact_device_stream=*/nullptr, GetPairingCallback(),
        GetInvalidatedPairingCallback(), GetEventCallback(),
        /*must_support_ctap=*/true);

    ReplaceDiscoveryFactory(
        std::make_unique<DiscoveryFactory>(std::move(discovery)));
    MaybeExpectDiscoveryWithScanCallback();

    const std::vector<uint8_t> contact_id(/*count=*/200, /*value=*/1);
    std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
        device::cablev2::authenticator::TransactFromQRCode(
            device::cablev2::authenticator::NewMockPlatform(
                &virtual_device_, mock_bluetooth_adapter_,
                /*observer=*/nullptr),
            base::BindLambdaForTesting(
                [&]() { return network_context_.get(); }),
            root_secret_, "Test Authenticator", zero_qr_secret_,
            peer_identity_x962_, contact_id);

    EXPECT_EQ(AuthenticatorMakeCredential().status,
              AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(pairings_.size(), 1u);

    // Now do a pairing-based exchange. Generate a random request type hint to
    // ensure that all values work.
    device::FidoRequestType request_type =
        device::FidoRequestType::kMakeCredential;
    std::string expected_request_type_string = "mc";
    if (base::RandDouble() < 0.5) {
      request_type = device::FidoRequestType::kGetAssertion;
      expected_request_type_string = "ga";
    }

    auto callback_and_event_stream = device::cablev2::Discovery::EventStream<
        std::unique_ptr<device::cablev2::Pairing>>::New();
    discovery = std::make_unique<device::cablev2::Discovery>(
        request_type,
        base::BindLambdaForTesting([&]() { return network_context_.get(); }),
        qr_generator_key_, std::move(callback_and_event_stream.second),
        GetPairingCallback(), GetInvalidatedPairingCallback(),
        GetEventCallback(), /*must_support_ctap=*/true);

    maybe_contact_phones_callback_ = base::BindLambdaForTesting([&]() {
      callback_and_event_stream.first.Run(
          std::make_unique<device::cablev2::Pairing>(*pairings_[0]));
    });

    const std::array<uint8_t, device::cablev2::kRoutingIdSize> routing_id = {0};
    bool contact_callback_IsReady = false;
    // When the |cablev2::Discovery| starts it'll make a connection to the
    // tunnel service with the contact ID from the pairing data. This will be
    // handled by the |TestNetworkContext| and turned into a call to
    // |contact_callback_|. This simulates the tunnel server sending a cloud
    // message to a phone. Given the information from the connection, a
    // transaction can be created.
    contact_callback_ = base::BindLambdaForTesting(
        [this, &transaction, routing_id, contact_id, &contact_callback_IsReady,
         &expected_request_type_string](
            base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
            base::span<const uint8_t, device::cablev2::kPairingIDSize>
                pairing_id,
            base::span<const uint8_t, device::cablev2::kClientNonceSize>
                client_nonce,
            const std::string& request_type_hint) -> void {
          contact_callback_IsReady = true;
          CHECK_EQ(request_type_hint, expected_request_type_string);
          transaction = device::cablev2::authenticator::TransactFromFCM(
              device::cablev2::authenticator::NewMockPlatform(
                  &virtual_device_, mock_bluetooth_adapter_,
                  /*observer=*/nullptr),
              base::BindLambdaForTesting(
                  [&]() { return network_context_.get(); }),
              root_secret_, routing_id, tunnel_id, pairing_id, client_nonce,
              contact_id);
        });

    ReplaceDiscoveryFactory(
        std::make_unique<DiscoveryFactory>(std::move(discovery)));
    MaybeExpectDiscoveryWithScanCallback();

    EXPECT_EQ(AuthenticatorMakeCredential().status,
              AuthenticatorStatus::SUCCESS);
    EXPECT_TRUE(contact_callback_IsReady);
  }

  void ResetNetworkService() {
    network_context_ = device::cablev2::NewMockTunnelServer(base::BindRepeating(
        &AuthenticatorCableV2Test::OnContact, base::Unretained(this)));
  }

  const std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRSecretSize> zero_qr_secret_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRSeedSize> zero_seed_ = {0};

  std::unique_ptr<network::mojom::NetworkContext> network_context_;
  uint8_t peer_identity_x962_[device::kP256X962Length] = {};
  device::VirtualCtap2Device virtual_device_{DeviceState(), DeviceConfig()};
  std::vector<std::unique_ptr<device::cablev2::Pairing>> pairings_;
  base::OnceCallback<void(
      base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
      base::span<const uint8_t, device::cablev2::kPairingIDSize> pairing_id,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      const std::string& request_type_hint)>
      contact_callback_;
  ContactWhenReadyContentBrowserClient browser_client_{
      base::BindRepeating(&AuthenticatorCableV2Test::MaybeContactPhones,
                          base::Unretained(this))};
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
  base::OnceClosure maybe_contact_phones_callback_;
  std::vector<Event> events_;

  scoped_refptr<device::cablev2::CableMockBluetoothAdapter>
      mock_bluetooth_adapter_;

 private:
  static VirtualCtap2Device::State* DeviceState() {
    VirtualCtap2Device::State* state = new VirtualCtap2Device::State;
    state->fingerprints_enrolled = true;
    state->default_backup_eligibility = true;
    return state;
  }

  static VirtualCtap2Device::Config DeviceConfig() {
    // `MockPlatform` uses a virtual device to answer requests, but it can't
    // handle the credential ID being omitted in responses.
    VirtualCtap2Device::Config ret;
    ret.include_credential_in_assertion_response =
        VirtualCtap2Device::Config::IncludeCredential::ALWAYS;
    ret.prf_support = true;
    ret.internal_account_chooser = true;
    ret.internal_uv_support = true;
    ret.always_uv = true;
    return ret;
  }
};

TEST_F(AuthenticatorCableV2Test, QRBasedWithNoPairing) {
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion,
      base::BindLambdaForTesting([&]() { return network_context_.get(); }),
      qr_generator_key_,
      /*contact_device_stream=*/nullptr, GetPairingCallback(),
      GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));
  MaybeExpectDiscoveryWithScanCallback();

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(
              &virtual_device_, mock_bluetooth_adapter_,
              /*observer=*/nullptr),
          base::BindLambdaForTesting([&]() { return network_context_.get(); }),
          root_secret_, "Test Authenticator", zero_qr_secret_,
          peer_identity_x962_,
          /*contact_id=*/std::nullopt);

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 0u);
}

TEST_F(AuthenticatorCableV2Test, HandshakeError) {
  // A handshake error should be fatal to the request with
  // `kHybridTransportError`.
  auto network_context_factory =
      base::BindLambdaForTesting([&]() { return network_context_.get(); });
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion, network_context_factory,
      qr_generator_key_,
      /*contact_device_stream=*/nullptr, GetPairingCallback(),
      GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));
  MaybeExpectDiscoveryWithScanCallback();

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::NewHandshakeErrorDevice(
          device::cablev2::authenticator::NewMockPlatform(
              &virtual_device_, mock_bluetooth_adapter_,
              /*observer=*/nullptr),
          network_context_factory, zero_qr_secret_);

  FailureReasonFuture failure_reason_future;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_future.GetCallback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  TestMakeCredentialFuture future;
  authenticator->MakeCredential(GetTestPublicKeyCredentialCreationOptions(),
                                future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, std::get<0>(future.Get()));

  ASSERT_TRUE(failure_reason_future.IsReady());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kHybridTransportError,
            failure_reason_future.Get());
}

// Test having the network service crash between creating a discovery and
// performing a cable transaction. Regression test for crbug.com/332724843.
TEST_F(AuthenticatorCableV2Test, NetworkServiceCrash) {
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion,
      base::BindLambdaForTesting([&]() { return network_context_.get(); }),
      qr_generator_key_,
      /*contact_device_stream=*/nullptr, GetPairingCallback(),
      GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));
  MaybeExpectDiscoveryWithScanCallback();

  // Simulate the network service restarting.
  ResetNetworkService();

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(
              &virtual_device_, mock_bluetooth_adapter_,
              /*observer=*/nullptr),
          base::BindLambdaForTesting([&]() { return network_context_.get(); }),
          root_secret_, "Test Authenticator", zero_qr_secret_,
          peer_identity_x962_,
          /*contact_id=*/std::nullopt);

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 0u);
}

TEST_F(AuthenticatorCableV2Test, PairingBased) {
  DoPairingConnection();

  const std::vector<Event> kExpectedEvents = {
      // From the QR connection
      Event::kBLEAdvertReceived,
      Event::kReady,
      // From the paired connection
      Event::kBLEAdvertReceived,
      Event::kReady,
  };
  EXPECT_EQ(events_, kExpectedEvents);
}

TEST_F(AuthenticatorCableV2Test, PairingBasedWithConnectionSignal) {
  EnableConnectionSignalAtTunnelServer();
  DoPairingConnection();

  const std::vector<Event> kExpectedEvents = {
      // From the QR connection
      Event::kBLEAdvertReceived,
      Event::kReady,
      // From the paired connection
      Event::kPhoneConnected,
      Event::kBLEAdvertReceived,
      Event::kReady,
  };
  EXPECT_EQ(events_, kExpectedEvents);
}

static std::unique_ptr<device::cablev2::Pairing> DummyPairing() {
  auto ret = std::make_unique<device::cablev2::Pairing>();
  ret->tunnel_server_domain = device::cablev2::kTunnelServer;
  ret->contact_id = {1, 2, 3, 4, 5};
  ret->id = {6, 7, 8, 9};
  ret->secret = {10, 11, 12, 13};
  std::fill(ret->peer_public_key_x962.begin(), ret->peer_public_key_x962.end(),
            22);
  ret->name = __func__;

  return ret;
}

TEST_F(AuthenticatorCableV2Test, ContactIDDisabled) {
  // Passing |nullopt| as the callback here causes all contact IDs to be
  // rejected.
  network_context_ = device::cablev2::NewMockTunnelServer(std::nullopt);
  auto callback_and_event_stream = device::cablev2::Discovery::EventStream<
      std::unique_ptr<device::cablev2::Pairing>>::New();
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion,
      base::BindLambdaForTesting([&]() { return network_context_.get(); }),
      qr_generator_key_, std::move(callback_and_event_stream.second),
      GetPairingCallback(), GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));
  MaybeExpectDiscoveryWithScanCallback();

  maybe_contact_phones_callback_ =
      base::BindLambdaForTesting([&callback_and_event_stream]() {
        callback_and_event_stream.first.Run(DummyPairing());
      });

  pairings_.emplace_back(DummyPairing());
  ASSERT_EQ(pairings_.size(), 1u);
  EXPECT_EQ(AuthenticatorMakeCredentialAndWaitForTimeout(
                GetTestPublicKeyCredentialCreationOptions())
                .status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  // The pairing should be been erased because of the signal from the tunnel
  // server.
  ASSERT_EQ(pairings_.size(), 0u);
}

TEST_F(AuthenticatorCableV2Test, LateLinking) {
  auto network_context_factory =
      base::BindLambdaForTesting([&]() { return network_context_.get(); });
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion, network_context_factory,
      qr_generator_key_,
      /*contact_device_stream=*/nullptr, GetPairingCallback(),
      GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));
  MaybeExpectDiscoveryWithScanCallback();

  const std::vector<uint8_t> contact_id(/*count=*/200, /*value=*/1);
  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::NewLateLinkingDevice(
          device::CtapDeviceResponseCode::kCtap2ErrOperationDenied,
          device::cablev2::authenticator::NewMockPlatform(
              &virtual_device_, mock_bluetooth_adapter_,
              /*observer=*/nullptr),
          network_context_factory, zero_qr_secret_, peer_identity_x962_);

  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);

  // There should not be any pairing at this point because the device shouldn't
  // have sent the information yet.
  EXPECT_EQ(pairings_.size(), 0u);

  // After 30 seconds, a pairing should have been recorded even though the
  // WebAuthn request has completed.
  task_environment()->FastForwardBy(base::Seconds(30));
  EXPECT_EQ(pairings_.size(), 1u);
}

// AuthenticatorCableV2AuthenticatorTest tests aspects of the authenticator
// implementation, rather than of the underlying caBLEv2 transport.
class AuthenticatorCableV2AuthenticatorTest
    : public AuthenticatorCableV2Test,
      public device::cablev2::authenticator::Observer {
 public:
  void SetUp() override {
    AuthenticatorCableV2Test::SetUp();

    auto discovery = std::make_unique<device::cablev2::Discovery>(
        device::FidoRequestType::kGetAssertion,
        base::BindLambdaForTesting([&]() { return network_context_.get(); }),
        qr_generator_key_,
        /*contact_device_stream=*/nullptr, GetPairingCallback(),
        GetInvalidatedPairingCallback(), GetEventCallback(),
        /*must_support_ctap=*/true);

    ReplaceDiscoveryFactory(
        std::make_unique<DiscoveryFactory>(std::move(discovery)));
    MaybeExpectDiscoveryWithScanCallback();

    transaction_ = device::cablev2::authenticator::TransactFromQRCode(
        device::cablev2::authenticator::NewMockPlatform(
            &virtual_device_, mock_bluetooth_adapter_, this),
        base::BindLambdaForTesting([&]() { return network_context_.get(); }),
        root_secret_, "Test Authenticator", zero_qr_secret_,
        peer_identity_x962_,
        /*contact_id=*/std::nullopt);
  }

 protected:
  // device::cablev2::authenticator::Observer
  void OnStatus(device::cablev2::authenticator::Platform::Status) override {}
  void OnCompleted(
      std::optional<device::cablev2::authenticator::Platform::Error> error)
      override {
    CHECK(!did_complete_);
    did_complete_ = true;
    error_ = error;
  }

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction_;
  bool did_complete_ = false;
  std::optional<device::cablev2::authenticator::Platform::Error> error_;
};

TEST_F(AuthenticatorCableV2AuthenticatorTest, GetAssertion) {
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials[0].transports.insert(
      device::FidoTransportProtocol::kHybrid);
  ASSERT_TRUE(virtual_device_.mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, options->relying_party_id));

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, MakeDiscoverableCredential) {
  auto options = GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->resident_key =
      device::ResidentKeyRequirement::kRequired;
  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);

  ASSERT_TRUE(did_complete_);
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ(*error_, device::cablev2::authenticator::Platform::Error::
                         DISCOVERABLE_CREDENTIALS_REQUEST);
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, EmptyAllowList) {
  auto options = GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();
  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);

  ASSERT_TRUE(did_complete_);
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ(*error_, device::cablev2::authenticator::Platform::Error::
                         DISCOVERABLE_CREDENTIALS_REQUEST);
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, PRFMakeCredential) {
  auto options = GetTestPublicKeyCredentialCreationOptions();
  options->prf_enable = true;

  const auto result = AuthenticatorMakeCredential(std::move(options));

  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(result.response->echo_prf);
  EXPECT_TRUE(result.response->prf);
}

static std::vector<uint8_t> HashPRFInput(base::span<const uint8_t> input) {
  crypto::hash::Hasher hasher(crypto::hash::kSha256);
  // clang-format off
  constexpr auto kPrefix = std::to_array<uint8_t>({
      'W', 'e', 'b', 'A', 'u', 't', 'h', 'n',
      ' ', 'P', 'R', 'F',
      0x00,
  });
  // clang-format on
  hasher.Update(kPrefix);
  hasher.Update(input);
  std::array<uint8_t, crypto::hash::kSha256Size> result;
  hasher.Finish(result);
  return base::ToVector(result);
}

static std::tuple<PublicKeyCredentialRequestOptionsPtr,
                  std::vector<uint8_t>,
                  std::vector<uint8_t>>
BuildPRFGetAssertion(device::VirtualCtap2Device& virtual_device,
                     bool use_eval_by_credential) {
  const std::vector<uint8_t> input1(32, 1);
  const std::vector<uint8_t> input2(32, 2);
  const std::vector<uint8_t> salt1 = HashPRFInput(input1);
  const std::vector<uint8_t> salt2 = HashPRFInput(input2);
  const std::array<uint8_t, 32> key1 = {1};
  const std::array<uint8_t, 32> key2 = {2};
  const std::array<uint8_t, 32> output1 = crypto::hmac::SignSha256(key2, salt1);
  const std::array<uint8_t, 32> output2 = crypto::hmac::SignSha256(key2, salt2);
  auto options = GetTestPublicKeyCredentialRequestOptions();

  CHECK(virtual_device.mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, options->relying_party_id));
  virtual_device.mutable_state()
      ->registrations.begin()
      ->second.hmac_key.emplace(key1, key2);

  std::vector<blink::mojom::PRFValuesPtr> prf_inputs;
  auto prf_value = blink::mojom::PRFValues::New();
  prf_value->first = input1;
  prf_value->second = input2;
  if (use_eval_by_credential) {
    prf_value->id = options->allow_credentials[0].id;
  }
  prf_inputs.emplace_back(std::move(prf_value));

  options->allow_credentials[0].transports.insert(
      device::FidoTransportProtocol::kHybrid);
  options->extensions->prf = true;
  options->extensions->prf_inputs = std::move(prf_inputs);
  options->user_verification = device::UserVerificationRequirement::kRequired;

  return std::make_tuple(std::move(options), base::ToVector(output1),
                         base::ToVector(output2));
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, PRFGetAssertion) {
  PublicKeyCredentialRequestOptionsPtr options;
  std::vector<uint8_t> output1, output2;
  std::tie(options, output1, output2) = BuildPRFGetAssertion(
      virtual_device_, /* use_eval_by_credential= */ false);

  const auto result = AuthenticatorGetAssertion(std::move(options));

  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(result.response->extensions->echo_prf);
  EXPECT_TRUE(result.response->extensions->prf_results);
  EXPECT_EQ(result.response->extensions->prf_results->first, output1);
  ASSERT_TRUE(result.response->extensions->prf_results->second.has_value());
  EXPECT_EQ(*result.response->extensions->prf_results->second, output2);
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, PRFGetAssertionByCredential) {
  PublicKeyCredentialRequestOptionsPtr options;
  std::vector<uint8_t> output1, output2;
  std::tie(options, output1, output2) =
      BuildPRFGetAssertion(virtual_device_, /* use_eval_by_credential= */ true);

  const auto result = AuthenticatorGetAssertion(std::move(options));

  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(result.response->extensions->echo_prf);
  EXPECT_TRUE(result.response->extensions->prf_results);
  EXPECT_EQ(result.response->extensions->prf_results->first, output1);
  ASSERT_TRUE(result.response->extensions->prf_results->second.has_value());
  EXPECT_EQ(*result.response->extensions->prf_results->second, output2);
}
}  // namespace
}  // namespace content
