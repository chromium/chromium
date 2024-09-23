// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_supplemental_pub_keys_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_supplemental_pub_keys_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cable_authentication_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_desktop_client_override.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

using V8Context = blink::V8IdentityCredentialRequestOptionsContext;
using blink::mojom::blink::RpContext;

const uint8_t kSample[] = {1, 2, 3, 4, 5, 6};

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
  return std::equal(data, data + arg->ByteLength(), std::begin(vector));
}

MATCHER_P(UnionDOMArrayBufferOrViewEqualTo, vector, "") {
  blink::DOMArrayBuffer* buffer = arg->IsArrayBuffer()
                                      ? arg->GetAsArrayBuffer()
                                      : arg->GetAsArrayBufferView()->buffer();
  if (buffer->ByteLength() != std::size(vector)) {
    return false;
  }
  uint8_t* data = (uint8_t*)buffer->Data();
  return std::equal(data, data + buffer->ByteLength(), std::begin(vector));
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

blink::RemoteDesktopClientOverride* blinkRemoteDesktopOverride(String origin) {
  blink::RemoteDesktopClientOverride* remote_desktop_client_override =
      blink::RemoteDesktopClientOverride::Create();
  remote_desktop_client_override->setOrigin(origin);
  return remote_desktop_client_override;
}

blink::mojom::blink::RemoteDesktopClientOverridePtr mojoRemoteDesktopOverride(
    String origin_string) {
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
      Vector({String::FromUTF8(attestation_format)}));
  supplemental_pub_keys->setScopes(
      Vector({String::FromUTF8("device"), String::FromUTF8("provider")}));
  blink_type->setSupplementalPubKeys(supplemental_pub_keys);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  auto expected = blink::mojom::blink::SupplementalPubKeysRequest::New(
      /*device_scope_requested=*/true,
      /*provider_scope_requested=*/true,
      blink::mojom::blink::AttestationConveyancePreference::INDIRECT,
      Vector<WTF::String>({WTF::String::FromUTF8(attestation_format)}));
  ASSERT_EQ(*(mojo_type->supplemental_pub_keys), *expected);
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
  std::copy(data, data + size, std::back_insert_iterator(vector));
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

}  // namespace mojo
