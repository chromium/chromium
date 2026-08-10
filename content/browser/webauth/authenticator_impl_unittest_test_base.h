// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_UNITTEST_TEST_BASE_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_UNITTEST_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/webauth/authenticator_common_impl.h"
#include "content/browser/webauth/authenticator_request_outcome_enums.h"
#include "content/browser/webauth/authenticator_test_base.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/large_blob.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

class WebAuthenticationDelegate;

using InterestingFailureReason =
    AuthenticatorRequestClientDelegate::InterestingFailureReason;
using FailureReasonFuture = base::test::TestFuture<InterestingFailureReason>;

using TestGetClientCapabilityFuture = base::test::TestFuture<
    std::vector<blink::mojom::WebAuthnClientCapabilityPtr>>;
using TestIsUvpaaFuture = base::test::TestFuture<bool>;
using TestMakeCredentialFuture =
    base::test::TestFuture<blink::mojom::AuthenticatorStatus,
                           blink::mojom::MakeCredentialAuthenticatorResponsePtr,
                           blink::mojom::WebAuthnDOMExceptionDetailsPtr>;
using TestGetCredentialFuture =
    base::test::TestFuture<blink::mojom::GetCredentialResponsePtr>;
using TestRequestStartedFuture = base::test::TestFuture<void>;
using TestReportFuture =
    base::test::TestFuture<blink::mojom::AuthenticatorStatus,
                           blink::mojom::WebAuthnDOMExceptionDetailsPtr>;

class AuthenticatorImplTest : public AuthenticatorTestBase {
 public:
  static constexpr base::TimeDelta kTestTimeout = base::Minutes(1);

  static constexpr char kTestOrigin1[] = "https://a.google.com";
  static constexpr char kTestOrigin2[] = "https://acme.org";
  static constexpr char kDifferentTestRelyingPartyId[] = "different-rp.com";
  static constexpr char kExtensionScheme[] = "chrome-extension";
  static constexpr char kCorpCrdOrigin[] =
      "https://remotedesktop.corp.google.com";

  static constexpr char kTestPIN[] = "1234";
  static constexpr char16_t kTestPIN16[] = u"1234";

  static const uint8_t kTestChallengeBytes[];

  static std::vector<uint8_t> GetTestChallengeBytes();
  static blink::mojom::PublicKeyCredentialReportOptionsPtr
  GetTestPublicKeyCredentialReportOptions();
  static device::AuthenticatorData AuthDataFromMakeCredentialResponse(
      const blink::mojom::MakeCredentialAuthenticatorResponsePtr& response);
  static bool HasUV(
      const blink::mojom::MakeCredentialAuthenticatorResponsePtr& response);
  static bool HasUV(
      const blink::mojom::GetAssertionAuthenticatorResponsePtr& response);
  static url::Origin GetTestOrigin();
  static std::string GetTestClientDataJSON(
      webauthn::ClientDataRequestType type);
  static device::LargeBlob CompressLargeBlob(base::span<const uint8_t> blob);
  static std::vector<uint8_t> UncompressLargeBlob(device::LargeBlob blob);
  static device::AttestationConveyancePreference
  ConvertAttestationConveyancePreference(
      blink::mojom::AttestationConveyancePreference in);

 protected:
  AuthenticatorImplTest();
  ~AuthenticatorImplTest() override;

  void SetUp() override;

  void SetBluetoothLESupported(bool supported);
  void NavigateAndCommit(const GURL& url);
  mojo::Remote<blink::mojom::Authenticator> ConnectToAuthenticator();
  bool AuthenticatorIsUvpaa();

  using ClientCapabilitiesList =
      std::vector<blink::mojom::WebAuthnClientCapabilityPtr>;
  ClientCapabilitiesList AuthenticatorGetClientCapabilities();

  void ExpectCapability(
      const std::vector<blink::mojom::WebAuthnClientCapabilityPtr>&
          capabilities,
      std::string_view capability_name,
      std::optional<bool> supported);

  bool AuthenticatorIsConditionalMediationAvailable();

  struct MakeCredentialResult {
    blink::mojom::AuthenticatorStatus status;
    blink::mojom::MakeCredentialAuthenticatorResponsePtr response;
  };
  MakeCredentialResult AuthenticatorMakeCredential();
  MakeCredentialResult AuthenticatorMakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options);
  MakeCredentialResult AuthenticatorMakeCredentialAndWaitForTimeout(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options);

  struct GetAssertionResult {
    blink::mojom::AuthenticatorStatus status;
    blink::mojom::GetAssertionAuthenticatorResponsePtr response;
  };
  GetAssertionResult AuthenticatorGetAssertion();
  GetAssertionResult AuthenticatorGetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options);
  GetAssertionResult AuthenticatorGetAssertionAndWaitForTimeout(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options);
  GetAssertionResult AuthenticatorGetCredential(
      blink::mojom::GetCredentialOptionsPtr options);

  blink::mojom::AuthenticatorStatus AuthenticatorReport(
      blink::mojom::PublicKeyCredentialReportOptionsPtr options);

  blink::mojom::AuthenticatorStatus TryAuthenticationWithAppId(
      std::string_view origin,
      std::string_view appid);
  blink::mojom::AuthenticatorStatus TryRegistrationWithAppIdExclude(
      std::string_view origin,
      std::string_view appid_exclude);

  ukm::TestUkmRecorder* GetTestUkmRecorder();
  void VerifyGetAssertionOutcomeUkm(uint32_t index,
                                    GetAssertionOutcome outcome,
                                    AuthenticationRequestMode mode);
  void VerifyMakeCredentialOutcomeUkm(uint32_t index,
                                      MakeCredentialOutcome outcome,
                                      AuthenticationRequestMode mode);

  void InjectVirtualAuthenticatorForAllTransports();

  scoped_refptr<::testing::NiceMock<device::MockBluetoothAdapter>>
      mock_adapter_ = base::MakeRefCounted<
          ::testing::NiceMock<device::MockBluetoothAdapter>>();

 private:
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalOverrideValues>
      bluetooth_global_values_ =
          device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  data_decoder::test::InProcessDataDecoder data_decoder_service_;
  url::ScopedSchemeRegistryForTests scoped_registry_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

// UVAuthenticatorImplTest helper classes
class UVTestAuthenticatorClientDelegate
    : public DefaultAuthenticatorRequestClientDelegate {
 public:
  UVTestAuthenticatorClientDelegate(bool* collected_pin,
                                    uint32_t* min_pin_length,
                                    bool* did_bio_enrollment,
                                    bool cancel_bio_enrollment,
                                    bool block_request_on_failure_once);
  ~UVTestAuthenticatorClientDelegate() override;

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override;

  bool SupportsPIN() const override;

  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override;

  void StartBioEnrollment(base::OnceClosure next_callback) override;

  void OnSampleCollected(int remaining_samples) override;

 private:
  const raw_ptr<bool> collected_pin_;
  const raw_ptr<uint32_t> min_pin_length_;
  base::OnceClosure bio_callback_;
  const raw_ptr<bool> did_bio_enrollment_;
  const bool cancel_bio_enrollment_;
  bool block_request_on_failure_once_;
};

class UVTestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  UVTestAuthenticatorContentBrowserClient();
  ~UVTestAuthenticatorContentBrowserClient() override;

  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override;
  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override;

  TestWebAuthenticationDelegate* GetTestWebAuthenticationDelegate();

  TestWebAuthenticationDelegate web_authentication_delegate;
  bool collected_pin = false;
  uint32_t min_pin_length = 0;
  bool did_bio_enrollment = false;
  bool cancel_bio_enrollment = false;
  bool block_request_on_failure_once = false;
};

class UVAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  UVAuthenticatorImplTest();
  UVAuthenticatorImplTest(const UVAuthenticatorImplTest&) = delete;
  UVAuthenticatorImplTest& operator=(const UVAuthenticatorImplTest&) = delete;
  ~UVAuthenticatorImplTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  static blink::mojom::PublicKeyCredentialCreationOptionsPtr
  make_credential_options(device::UserVerificationRequirement uv =
                              device::UserVerificationRequirement::kRequired,
                          bool exclude_credentials = false,
                          bool appid_exclude = false);

  static blink::mojom::PublicKeyCredentialRequestOptionsPtr
  get_credential_options(device::UserVerificationRequirement uv =
                             device::UserVerificationRequirement::kRequired);

  UVTestAuthenticatorContentBrowserClient test_client_;

 private:
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

// AuthenticatorImplRequestDelegateTest helper classes
class MockAuthenticatorRequestDelegateObserver
    : public TestAuthenticatorRequestDelegate {
 public:
  using InterestingFailureReasonCallback =
      base::OnceCallback<void(InterestingFailureReason)>;

  explicit MockAuthenticatorRequestDelegateObserver(
      InterestingFailureReasonCallback failure_reasons_callback =
          base::DoNothing());
  MockAuthenticatorRequestDelegateObserver(
      const MockAuthenticatorRequestDelegateObserver&) = delete;
  MockAuthenticatorRequestDelegateObserver& operator=(
      const MockAuthenticatorRequestDelegateObserver&) = delete;
  ~MockAuthenticatorRequestDelegateObserver() override;

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override;

  MOCK_METHOD1(
      OnTransportAvailabilityEnumerated,
      void(device::FidoRequestHandlerBase::TransportAvailabilityInfo data));
  MOCK_METHOD1(EmbedderControlsAuthenticatorDispatch,
               bool(const device::FidoAuthenticator&));
  MOCK_METHOD1(FidoAuthenticatorAdded, void(const device::FidoAuthenticator&));
  MOCK_METHOD1(FidoAuthenticatorRemoved, void(std::string_view));

 private:
  InterestingFailureReasonCallback failure_reasons_callback_;
};

class FakeAuthenticatorCommonImpl : public AuthenticatorCommonImpl {
 public:
  explicit FakeAuthenticatorCommonImpl(
      RenderFrameHost* render_frame_host,
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate);
  ~FakeAuthenticatorCommonImpl() override;

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  MaybeCreateRequestDelegate() override;

 private:
  std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate_;
};

class AuthenticatorImplRequestDelegateTest : public AuthenticatorImplTest {
 public:
  AuthenticatorImplRequestDelegateTest();
  ~AuthenticatorImplRequestDelegateTest() override;

  mojo::Remote<blink::mojom::Authenticator> ConnectToFakeAuthenticator(
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_UNITTEST_TEST_BASE_H_
