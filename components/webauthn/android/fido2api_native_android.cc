// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_array.h"
#include "components/cbor/reader.h"
#include "components/webauthn/android/jni_headers/Fido2Api_jni.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/public_key.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace webauthn {

// Parses a CTAP2 attestation[1] and extracts the
// parts that the browser provides via Javascript API [2]. Called
// Fido2Api.java when constructing the makeCredential reply.
//
// [1] https://www.w3.org/TR/webauthn/#attestation-object
// [2] https://w3c.github.io/webauthn/#sctn-public-key-easy
static jboolean JNI_Fido2Api_ParseAttestationObject(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& jattestation_object_bytes,
    const base::android::JavaParamRef<jobject>& out_result) {
  std::vector<uint8_t> attestation_object_bytes;
  JavaByteArrayToByteVector(env, jattestation_object_bytes,
                            &attestation_object_bytes);

  absl::optional<cbor::Value> attestation_object =
      cbor::Reader::Read(attestation_object_bytes);
  if (!attestation_object || !attestation_object->is_map()) {
    return false;
  }

  const cbor::Value::MapValue& map = attestation_object->GetMap();
  // See https://www.w3.org/TR/webauthn/#generating-an-attestation-object
  cbor::Value::MapValue::const_iterator it = map.find(cbor::Value("authData"));
  if (it == map.end() || !it->second.is_bytestring()) {
    return false;
  }
  const std::vector<uint8_t>& auth_data = it->second.GetBytestring();
  // See https://www.w3.org/TR/webauthn/#sec-authenticator-data
  CBS cbs;
  CBS_init(&cbs, auth_data.data(), auth_data.size());
  uint8_t flags;
  if (  // RP ID hash.
      !CBS_skip(&cbs, 32) || !CBS_get_u8(&cbs, &flags) ||
      // Check AT flag is set.
      ((flags >> 6) & 1) == 0 ||
      // Signature counter.
      !CBS_skip(&cbs, 4)) {
    return false;
  }

  const auto result = device::AttestedCredentialData::ConsumeFromCtapResponse(
      base::span<const uint8_t>(CBS_data(&cbs), CBS_len(&cbs)));
  if (!result) {
    return false;
  }

  ScopedJavaLocalRef<jbyteArray> auth_data_java(
      ToJavaByteArray(env, auth_data));

  const device::PublicKey* pub_key = result->first.public_key();
  const absl::optional<std::vector<uint8_t>>& der_bytes(pub_key->der_bytes);
  ScopedJavaLocalRef<jbyteArray> spki_java;
  if (der_bytes) {
    spki_java.Reset(ToJavaByteArray(env, *der_bytes));
  }

  Java_AttestationObjectParts_setAll(env, out_result, auth_data_java, spki_java,
                                     pub_key->algorithm);

  return true;
}

// JavaByteArrayToSpan returns a span that aliases |data|. Be aware that the
// reference for |data| must outlive the span.
static base::span<const uint8_t> JavaByteArrayToSpan(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& data) {
  if (data.is_null()) {
    return base::span<const uint8_t>();
  }

  const size_t data_len = env->GetArrayLength(data);
  const jbyte* data_bytes = env->GetByteArrayElements(data, /*iscopy=*/nullptr);
  return base::as_bytes(base::make_span(data_bytes, data_len));
}

static ScopedJavaLocalRef<jbyteArray>
JNI_Fido2Api_GetDevicePublicKeyFromAuthenticatorData(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& jauthenticator_data) {
  absl::optional<device::AuthenticatorData> auth_data =
      device::AuthenticatorData::DecodeAuthenticatorData(
          JavaByteArrayToSpan(env, jauthenticator_data));
  if (!auth_data) {
    return nullptr;
  }

  const absl::optional<cbor::Value>& extensions = auth_data->extensions();
  if (!extensions) {
    return nullptr;
  }

  const cbor::Value::MapValue& extensions_map = extensions->GetMap();
  const auto it =
      extensions_map.find(cbor::Value(device::kExtensionDevicePublicKey));
  if (it == extensions_map.end() || !it->second.is_bytestring()) {
    return nullptr;
  }

  return ToJavaByteArray(env, it->second.GetBytestring());
}

}  // namespace webauthn
