// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_array.h"
#include "components/cbor/values.h"
#include "components/webauthn/android/jni_headers/Fido2Api_jni.h"
#include "device/fido/attestation_object.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"

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
    jboolean attestation_acceptable,
    const base::android::JavaParamRef<jobject>& out_result) {
  std::vector<uint8_t> attestation_object_bytes;
  JavaByteArrayToByteVector(env, jattestation_object_bytes,
                            &attestation_object_bytes);
  absl::optional<device::AttestationObject::ResponseFields> fields =
      device::AttestationObject::ParseForResponseFields(
          std::move(attestation_object_bytes), attestation_acceptable);
  if (!fields) {
    return false;
  }

  ScopedJavaLocalRef<jbyteArray> spki_java;
  if (fields->public_key_der) {
    spki_java.Reset(ToJavaByteArray(env, *fields->public_key_der));
  }

  Java_AttestationObjectParts_setAll(
      env, out_result, ToJavaByteArray(env, fields->authenticator_data),
      spki_java, fields->public_key_algo,
      ToJavaByteArray(env, fields->attestation_object_bytes));

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
