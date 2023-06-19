// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"

#include "mojo/public/cpp/bindings/type_converter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_device_public_key_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cable_authentication_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_desktop_client_override.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

using V8Context = blink::V8IdentityCredentialRequestOptionsContext;
using blink::mojom::blink::RpContext;

const uint8_t kSample[] = {1, 2, 3, 4, 5, 6};

// This constant is the result of hashing the prefix "WebAuthn\x00" with
// kSample: sha256("WebAuthn\x00\x01\x02\x03\x04\x05\x06")
const uint8_t kSamplePrfHashed[] = {
    0x36, 0x43, 0xbb, 0x85, 0x29, 0xcd, 0xab, 0x07, 0xe3, 0x2d, 0x2e,
    0x0d, 0xb9, 0xb7, 0x60, 0x56, 0x39, 0x9a, 0x58, 0x29, 0x02, 0x9c,
    0xfa, 0x5c, 0xb8, 0x1c, 0x6d, 0x09, 0x30, 0x8c, 0x77, 0x29};

static blink::V8UnionArrayBufferOrArrayBufferView* arrayBufferOrView(
    const uint8_t* data,
    size_t size);
static Vector<uint8_t> vectorOf(const uint8_t* data, size_t size);

TEST(CredentialManagerTypeConvertersTest, RpContextTest) {
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

const char* attestationFormat = "indirect";

TEST(CredentialManagerTypeConvertersTest,
     AuthenticationExtensionsClientInputsTest_devicePublicKey) {
  blink::AuthenticationExtensionsClientInputs* blink_type =
      blink::AuthenticationExtensionsClientInputs::Create();
  blink::AuthenticationExtensionsDevicePublicKeyInputs* devicePublicKeyRequest =
      blink::AuthenticationExtensionsDevicePublicKeyInputs::Create();
  devicePublicKeyRequest->setAttestation("indirect");
  devicePublicKeyRequest->setAttestationFormats(
      Vector({String::FromUTF8(attestationFormat)}));
  blink_type->setDevicePubKey(devicePublicKeyRequest);

  blink::mojom::blink::AuthenticationExtensionsClientInputsPtr mojo_type =
      ConvertTo<blink::mojom::blink::AuthenticationExtensionsClientInputsPtr>(
          *blink_type);

  auto expected = blink::mojom::blink::DevicePublicKeyRequest::New(
      blink::mojom::blink::AttestationConveyancePreference::INDIRECT,
      Vector<WTF::String>({WTF::String::FromUTF8(attestationFormat)}));
  ASSERT_EQ(*(mojo_type->device_public_key), *expected);
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

  auto sample_vector = vectorOf(kSamplePrfHashed, std::size(kSamplePrfHashed));
  Vector<blink::mojom::blink::PRFValuesPtr> expected_prf_values;
  expected_prf_values.emplace_back(blink::mojom::blink::PRFValues::New(
      absl::optional<Vector<uint8_t>>(), sample_vector,
      absl::optional<Vector<uint8_t>>()));
  ASSERT_EQ(mojo_type->prf_inputs[0]->first, expected_prf_values[0]->first);
}

static blink::V8UnionArrayBufferOrArrayBufferView* arrayBufferOrView(
    const uint8_t* data,
    size_t size) {
  blink::DOMArrayBuffer* dom_array = blink::DOMArrayBuffer::Create(data, size);
  return blink::MakeGarbageCollected<
      blink::V8UnionArrayBufferOrArrayBufferView>(std::move(dom_array));
}

static Vector<uint8_t> vectorOf(const uint8_t* data, size_t size) {
  Vector<uint8_t> vector;
  std::copy(data, data + size, std::back_insert_iterator(vector));
  return vector;
}

}  // namespace mojo
