// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "device/fido/public/fido_constants.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/webid/login_status_account.h"
#include "third_party/blink/public/common/webid/login_status_options.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_payment_browser_bound_signature.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_payment_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_payment_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_supplemental_pub_keys_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_supplemental_pub_keys_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cable_authentication_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_account.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_login_status_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_desktop_client_override.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

using V8Context = blink::V8IdentityCredentialRequestOptionsContext;
using blink::Vector;
using blink::mojom::blink::RpContext;

const uint8_t kSample[] = {1, 2, 3, 4, 5, 6};

const int32_t kCoseAlgorithmEs256 =
    base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256);
const int32_t kCoseAlgorithmRs256 =
    base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kRs256);

static blink::V8UnionArrayBufferOrArrayBufferView* arrayBufferOrView(
    const uint8_t* data,
    size_t size);
static Vector<uint8_t> vectorOf(const uint8_t* data, size_t size);

TEST(CredentialManagerTypeConvertersTest, RpContextTest) {
  blink::test::TaskEnvironment task_environment;
  EXPECT_EQ(RpContext::kSignIn,
            ConvertTo<RpContext>(V8Context(V8Context::Enum::kSignin)));
  EXPECT_EQ(RpContext::kSignUp,
            ConvertTo<RpContext>(V8Context(V8Context::Enum::kSignup)));
  EXPECT_EQ(RpContext::kUse,
            ConvertTo<RpContext>(V8Context(V8Context::Enum::kUse)));
  EXPECT_EQ(RpContext::kContinue,
            ConvertTo<RpContext>(V8Context(V8Context::Enum::kContinue)));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_appidNotSet) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->echo_appid_extension = false;

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_FALSE(blink_type->hasAppid());
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_appidSetTrue) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->echo_appid_extension = true;
  mojo_type->appid_extension = true;

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasAppid());
  EXPECT_TRUE(blink_type->appid());
}

#if BUILDFLAG(IS_ANDROID)
TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_userVerificationMethods) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->echo_user_verification_methods = true;
  mojo_type->user_verification_methods =
      Vector<blink::mojom::blink::UvmEntryPtr>();
  mojo_type->user_verification_methods->emplace_back(
      blink::mojom::blink::UvmEntry::New(/*user_verification_method=*/1,
                                         /*key_protection_type=*/2,
                                         /*matcher_protection_type=*/3));
  mojo_type->user_verification_methods->emplace_back(
      blink::mojom::blink::UvmEntry::New(/*user_verification_method=*/4,
                                         /*key_protection_type=*/5,
                                         /*matcher_protection_type=*/6));

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasUvm());
  EXPECT_THAT(blink_type->uvm(),
              ::testing::ElementsAre(Vector<uint32_t>{1, 2, 3},
                                     Vector<uint32_t>{4, 5, 6}));
}
#endif

MATCHER_P(DOMArrayBufferEqualTo, vector, "") {
  if (arg->ByteLength() != std::size(vector)) {
    return false;
  }
  uint8_t* data = (uint8_t*)arg->Data();
  return std::equal(data, UNSAFE_TODO(data + arg->ByteLength()),
                    std::begin(vector));
}

MATCHER_P(UnionDOMArrayBufferOrViewEqualTo, vector, "") {
  blink::DOMArrayBuffer* buffer = arg->IsArrayBuffer()
                                      ? arg->GetAsArrayBuffer()
                                      : arg->GetAsArrayBufferView()->buffer();
  if (buffer->ByteLength() != std::size(vector)) {
    return false;
  }
  uint8_t* data = (uint8_t*)buffer->Data();
  return std::equal(data, UNSAFE_TODO(data + buffer->ByteLength()),
                    std::begin(vector));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_largeBlobEmpty) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->echo_large_blob = true;

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasLargeBlob());
  EXPECT_FALSE(blink_type->largeBlob()->hasBlob());
  EXPECT_FALSE(blink_type->largeBlob()->hasWritten());
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_largeBlobRead) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->echo_large_blob = true;
  mojo_type->large_blob = Vector<uint8_t>({1, 2, 3});

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasLargeBlob());
  EXPECT_THAT(blink_type->largeBlob()->blob(),
              DOMArrayBufferEqualTo(Vector<uint8_t>{1, 2, 3}));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_largeBlobWritten) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->echo_large_blob = true;
  mojo_type->echo_large_blob_written = true;
  mojo_type->large_blob_written = true;

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasLargeBlob());
  EXPECT_TRUE(blink_type->largeBlob()->written());
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_credBlob) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->get_cred_blob = Vector<uint8_t>{1, 2, 3};

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasGetCredBlob());
  EXPECT_THAT(blink_type->getCredBlob(),
              DOMArrayBufferEqualTo(Vector<uint8_t>{1, 2, 3}));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_supplementalPubKeys) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->supplemental_pub_keys =
      blink::mojom::blink::SupplementalPubKeysResponse::New(
          /*signatures=*/Vector<Vector<uint8_t>>{{1, 2, 3}, {4, 5, 6}});

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasSupplementalPubKeys());
  ASSERT_EQ(blink_type->supplementalPubKeys()->signatures().size(), 2u);
  EXPECT_THAT(blink_type->supplementalPubKeys()->signatures()[0],
              DOMArrayBufferEqualTo(Vector<uint8_t>{1, 2, 3}));
  EXPECT_THAT(blink_type->supplementalPubKeys()->signatures()[1],
              DOMArrayBufferEqualTo(Vector<uint8_t>{4, 5, 6}));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_payment) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->payment =
      blink::mojom::blink::AuthenticationExtensionsPaymentResponse::New(
          /*browser_bound_signature=*/Vector<uint8_t>{1, 2, 3});

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasPayment());
  EXPECT_TRUE(blink_type->payment()->hasBrowserBoundSignature());
  EXPECT_THAT(blink_type->payment()->browserBoundSignature()->signature(),
              DOMArrayBufferEqualTo(Vector<uint8_t>{1, 2, 3}));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_prf) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->echo_prf = true;
  mojo_type->prf_results = blink::mojom::blink::PRFValues::New(
      /*id=*/std::nullopt,
      /*first=*/Vector<uint8_t>{1, 2, 3},
      /*second=*/std::nullopt);

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasPrf());
  EXPECT_TRUE(blink_type->prf()->hasResults());
  blink::AuthenticationExtensionsPRFValues* prf_results =
      blink_type->prf()->results();
  EXPECT_TRUE(prf_results->hasFirst());
  EXPECT_THAT(prf_results->first(),
              UnionDOMArrayBufferOrViewEqualTo(Vector<uint8_t>{1, 2, 3}));
  EXPECT_FALSE(prf_results->hasSecond());
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientOutputs_prfWithSecond) {
  auto mojo_type =
      blink::mojom::blink::AuthenticationExtensionsClientOutputs::New();
  mojo_type->echo_prf = true;
  mojo_type->prf_results = blink::mojom::blink::PRFValues::New(
      /*id=*/std::nullopt,
      /*first=*/Vector<uint8_t>{1, 2, 3},
      /*second=*/Vector<uint8_t>{4, 5, 6});

  auto* blink_type =
      ConvertTo<blink::AuthenticationExtensionsClientOutputs*>(mojo_type);

  EXPECT_TRUE(blink_type->hasPrf());
  EXPECT_TRUE(blink_type->prf()->hasResults());
  blink::AuthenticationExtensionsPRFValues* blink_prf_values =
      blink_type->prf()->results();
  EXPECT_TRUE(blink_prf_values->hasSecond());
  EXPECT_THAT(blink_prf_values->second(),
              UnionDOMArrayBufferOrViewEqualTo(Vector<uint8_t>{4, 5, 6}));
}

TEST(CredentialManagerTypeConvertersTest,
     PublicKeyCredentialRequestOptions_extensions) {
  blink::PublicKeyCredentialRequestOptions* blink_type =
      blink::PublicKeyCredentialRequestOptions::Create();
  blink_type->setExtensions(
      blink::AuthenticationExtensionsClientInputs::Create());
  blink_type->extensions()->setAppid("app-id");
  blink_type->setChallenge(arrayBufferOrView(kSample, std::size(kSample)));

  blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr mojo_type =
      ConvertTo<blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr>(
          *blink_type);

  auto sample_vector = vectorOf(kSample, std::size(kSample));
  ASSERT_EQ(mojo_type->extensions->appid, "app-id");
  ASSERT_EQ(mojo_type->challenge, sample_vector);
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_appid) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink_type->setAppid("app-id");

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  ASSERT_EQ(mojo_type->appid, "app-id");
}

#if BUILDFLAG(IS_ANDROID)
TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_uvm) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink_type->setUvm(true);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  ASSERT_EQ(mojo_type->user_verification_methods, true);
}
#endif

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_largeBlobWrite) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsLargeBlobInputs* large_blob =
      blink::AuthenticationExtensionsLargeBlobInputs::Create();
  large_blob->setWrite(arrayBufferOrView(kSample, std::size(kSample)));
  blink_type->setLargeBlob(large_blob);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  auto sample_vector = vectorOf(kSample, std::size(kSample));
  ASSERT_EQ(mojo_type->large_blob_write, sample_vector);
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_largeBlobRead) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsLargeBlobInputs* large_blob =
      blink::AuthenticationExtensionsLargeBlobInputs::Create();
  large_blob->setRead(true);
  blink_type->setLargeBlob(large_blob);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  ASSERT_EQ(mojo_type->large_blob_read, true);
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_hasCredBlob) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink_type->setGetCredBlob(true);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  ASSERT_EQ(mojo_type->get_cred_blob, true);
}

blink::RemoteDesktopClientOverride* blinkRemoteDesktopOverride(
    blink::String origin) {
  blink::RemoteDesktopClientOverride* remote_desktop_client_override =
      blink::RemoteDesktopClientOverride::Create();
  remote_desktop_client_override->setOrigin(origin);
  return remote_desktop_client_override;
}

blink::mojom::blink::RemoteDesktopClientOverridePtr mojoRemoteDesktopOverride(
    blink::String origin_string) {
  auto remote_desktop_client_override =
      blink::mojom::blink::RemoteDesktopClientOverride::New();
  auto origin = blink::SecurityOrigin::CreateFromString(origin_string);
  remote_desktop_client_override->origin = std::move(origin);
  return remote_desktop_client_override;
}

const char* kSampleOrigin = "https://example.com";

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_remoteDesktopClientOverride) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink_type->setRemoteDesktopClientOverride(
      blinkRemoteDesktopOverride(kSampleOrigin));

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  auto expected = mojoRemoteDesktopOverride(kSampleOrigin);
  ASSERT_TRUE(
      mojo_type->remote_desktop_client_override->origin->IsSameOriginWith(
          &*expected->origin));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_supplementalPubKeys) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsSupplementalPubKeysInputs*
      supplemental_pub_keys =
          blink::AuthenticationExtensionsSupplementalPubKeysInputs::Create();

  const char attestation_format[] = "format";
  supplemental_pub_keys->setAttestation("indirect");
  supplemental_pub_keys->setAttestationFormats(
      Vector({blink::String::FromUTF8(attestation_format)}));
  supplemental_pub_keys->setScopes(
      Vector({blink::String::FromUTF8("device"),
              blink::String::FromUTF8("provider")}));
  blink_type->setSupplementalPubKeys(supplemental_pub_keys);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  auto expected = blink::mojom::blink::SupplementalPubKeysRequest::New(
      /*device_scope_requested=*/true,
      /*provider_scope_requested=*/true,
      blink::mojom::blink::AttestationConveyancePreference::INDIRECT,
      Vector<blink::String>({blink::String::FromUTF8(attestation_format)}));
  ASSERT_EQ(*(mojo_type->supplemental_pub_keys), *expected);
}

static ::testing::Matcher<const mojo::InlinedStructPtr<
    blink::mojom::blink::PublicKeyCredentialParameters>>
EqPublicKeyCredentialParameters(blink::mojom::PublicKeyCredentialType type,
                                int32_t algorithm_identifier) {
  return ::testing::Pointee(::testing::AllOf(
      ::testing::Field(
          "type", &blink::mojom::blink::PublicKeyCredentialParameters::type,
          type),
      ::testing::Field("algorithm_identifier",
                       &blink::mojom::blink::PublicKeyCredentialParameters::
                           algorithm_identifier,
                       algorithm_identifier)));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_browserBoundPublicKeyCredential) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsPaymentInputs* blink_payment =
      blink::AuthenticationExtensionsPaymentInputs::Create();
  auto blink_cred_params =
      blink::HeapVector<blink::Member<blink::PublicKeyCredentialParameters>>();
  blink::PublicKeyCredentialParameters* blink_cred_params_1 =
      blink::PublicKeyCredentialParameters::Create();
  blink_cred_params_1->setType("public-key");
  blink_cred_params_1->setAlg(kCoseAlgorithmEs256);
  blink_cred_params.push_back(blink_cred_params_1);
  blink_payment->setBrowserBoundPubKeyCredParams(std::move(blink_cred_params));
  blink_type->setPayment(blink_payment);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  EXPECT_THAT(mojo_type->payment_browser_bound_key_parameters,
              ::testing::Optional(
                  ::testing::ElementsAre(EqPublicKeyCredentialParameters(
                      blink::mojom::PublicKeyCredentialType::PUBLIC_KEY,
                      kCoseAlgorithmEs256))));
}

TEST(CredentialManagerTypeConvertersTest,
     PublicKeyCredentialCreationOptions_browserBoundPublicKeyCredential) {
  blink::PublicKeyCredentialCreationOptions* blink_creation_options =
      blink::PublicKeyCredentialCreationOptions::Create();
  blink::PublicKeyCredentialRpEntity* blink_rp_entity =
      blink::PublicKeyCredentialRpEntity::Create();
  blink_rp_entity->setName("rp-name.test");
  blink_creation_options->setRp(blink_rp_entity);
  blink::PublicKeyCredentialUserEntity* blink_user =
      blink::PublicKeyCredentialUserEntity::Create();
  blink_user->setId(arrayBufferOrView(kSample, std::size(kSample)));
  blink_creation_options->setUser(blink_user);
  blink_creation_options->setChallenge(
      arrayBufferOrView(kSample, std::size(kSample)));

  blink::AuthenticationExtensionsClientInputs* blink_extensions =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsPaymentInputs* blink_payment =
      blink::AuthenticationExtensionsPaymentInputs::Create();
  auto blink_cred_params =
      blink::HeapVector<blink::Member<blink::PublicKeyCredentialParameters>>();
  blink::PublicKeyCredentialParameters* blink_cred_params_1 =
      blink::PublicKeyCredentialParameters::Create();
  blink_cred_params_1->setType("public-key");
  blink_cred_params_1->setAlg(kCoseAlgorithmEs256);
  blink_cred_params.push_back(blink_cred_params_1);
  blink_payment->setBrowserBoundPubKeyCredParams(std::move(blink_cred_params));
  blink_extensions->setPayment(blink_payment);
  blink_creation_options->setExtensions(blink_extensions);

  blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr
      mojo_creation_options =
          ConvertTo<blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr>(
              *blink_creation_options);

  ASSERT_TRUE(mojo_creation_options);
  EXPECT_THAT(mojo_creation_options->payment_browser_bound_key_parameters,
              ::testing::Optional(
                  ::testing::ElementsAre(EqPublicKeyCredentialParameters(
                      blink::mojom::PublicKeyCredentialType::PUBLIC_KEY,
                      kCoseAlgorithmEs256))));
}

TEST(
    CredentialManagerTypeConvertersTest,
    AuthenticationExtensionsClientInputsTest_browserBoundPublicKeyCredentialWhenEmpty) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsPaymentInputs* blink_payment =
      blink::AuthenticationExtensionsPaymentInputs::Create();
  blink_payment->setBrowserBoundPubKeyCredParams(
      blink::HeapVector<blink::Member<blink::PublicKeyCredentialParameters>>());
  blink_type->setPayment(blink_payment);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  EXPECT_THAT(mojo_type->payment_browser_bound_key_parameters,
              ::testing::Optional(::testing::ElementsAre(
                  EqPublicKeyCredentialParameters(
                      blink::mojom::PublicKeyCredentialType::PUBLIC_KEY,
                      kCoseAlgorithmEs256),
                  EqPublicKeyCredentialParameters(
                      blink::mojom::PublicKeyCredentialType::PUBLIC_KEY,
                      kCoseAlgorithmRs256))));
}

TEST(
    CredentialManagerTypeConvertersTest,
    AuthenticationExtensionsClientInputsTest_browserBoundPublicKeyCredentialWhenInvalidType) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsPaymentInputs* blink_payment =
      blink::AuthenticationExtensionsPaymentInputs::Create();
  auto blink_cred_params =
      blink::HeapVector<blink::Member<blink::PublicKeyCredentialParameters>>();
  blink::PublicKeyCredentialParameters* blink_cred_params_1 =
      blink::PublicKeyCredentialParameters::Create();
  blink_cred_params_1->setType("Not a valid public key credential type!");
  blink_cred_params_1->setAlg(kCoseAlgorithmEs256);
  blink_cred_params.push_back(blink_cred_params_1);
  blink_payment->setBrowserBoundPubKeyCredParams(std::move(blink_cred_params));
  blink_type->setPayment(blink_payment);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  EXPECT_THAT(mojo_type->payment_browser_bound_key_parameters,
              ::testing::Optional(::testing::IsEmpty()));
}

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_prfInputs) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsPRFInputs* prf_inputs =
      blink::AuthenticationExtensionsPRFInputs::Create();
  blink::AuthenticationExtensionsPRFValues* prf_values =
      blink::AuthenticationExtensionsPRFValues::Create();
  prf_values->setFirst(arrayBufferOrView(kSample, std::size(kSample)));
  prf_inputs->setEval(prf_values);
  blink_type->setPrf(prf_inputs);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  auto sample_vector = vectorOf(kSample, std::size(kSample));
  Vector<blink::mojom::blink::PRFValuesPtr> expected_prf_values;
  expected_prf_values.emplace_back(blink::mojom::blink::PRFValues::New(
      std::optional<Vector<uint8_t>>(), sample_vector,
      std::optional<Vector<uint8_t>>()));
  ASSERT_EQ(mojo_type->prf_inputs[0]->first, expected_prf_values[0]->first);
}

static blink::V8UnionArrayBufferOrArrayBufferView* arrayBufferOrView(
    const uint8_t* data,
    size_t size) {
  return blink::MakeGarbageCollected<
      blink::V8UnionArrayBufferOrArrayBufferView>(
      blink::DOMArrayBuffer::Create(UNSAFE_TODO(base::span(data, size))));
}

static Vector<uint8_t> vectorOf(const uint8_t* data, size_t size) {
  Vector<uint8_t> vector;
  std::copy(data, UNSAFE_TODO(data + size), std::back_insert_iterator(vector));
  return vector;
}

// Crash test for crbug.com/347715555.
TEST(CredentialManagerTypeConvertersTest, NoClientId) {
  blink::IdentityProviderRequestOptions* provider =
      blink::IdentityProviderRequestOptions::Create();
  provider->setConfigURL("any");
  blink::mojom::blink::IdentityProviderRequestOptionsPtr identity_provider =
      ConvertTo<blink::mojom::blink::IdentityProviderRequestOptionsPtr>(
          *provider);
  EXPECT_EQ(identity_provider->config->client_id, "");
}

TEST(CredentialManagerTypeConvertersTest, LoginStatusOptions) {
  auto* blink_account = blink::IdentityProviderAccount::Create();
  blink_account->setId("some-identifier");
  blink_account->setEmail("user@example.com");
  blink_account->setGivenName("User");
  blink_account->setName("User Fullname");
  blink_account->setPicture("https://example.com/user.png");

  blink::HeapVector<blink::Member<blink::IdentityProviderAccount>>
      blink_accounts;
  blink_accounts.push_back(blink_account);

  auto* blink_options = blink::LoginStatusOptions::Create();
  blink_options->setExpiration(12345U);
  blink_options->setAccounts(blink_accounts);

  blink::mojom::blink::LoginStatusOptionsPtr mojo_options =
      ConvertTo<blink::mojom::blink::LoginStatusOptionsPtr>(*blink_options);

  ASSERT_TRUE(mojo_options->expiration.has_value());
  ASSERT_EQ(mojo_options->expiration.value().InMilliseconds(), 12345U);
  ASSERT_EQ(mojo_options->accounts[0]->id, "some-identifier");
  ASSERT_EQ(mojo_options->accounts[0]->email, "user@example.com");
  ASSERT_EQ(mojo_options->accounts[0]->name, "User Fullname");
  ASSERT_EQ(mojo_options->accounts[0]->given_name, "User");
  ASSERT_TRUE(mojo_options->accounts[0]->picture.has_value());
  ASSERT_EQ(mojo_options->accounts[0]->picture->GetString(),
            "https://example.com/user.png");
}

TEST(CredentialManagerTypeConvertersTest,
     IdentityProviderAccountNoOptionalFields) {
  auto* blink_account = blink::IdentityProviderAccount::Create();

  blink_account->setId("some-identifier");
  blink_account->setEmail("user@example.com");
  blink_account->setName("User Fullname");

  blink::mojom::blink::LoginStatusAccountPtr mojo_account =
      ConvertTo<blink::mojom::blink::LoginStatusAccountPtr>(*blink_account);

  ASSERT_EQ(mojo_account->id, "some-identifier");
  ASSERT_EQ(mojo_account->email, "user@example.com");
  ASSERT_EQ(mojo_account->name, "User Fullname");
  ASSERT_TRUE(mojo_account->given_name.IsNull());
  ASSERT_FALSE(mojo_account->picture.has_value());
}

}  // namespace mojo
