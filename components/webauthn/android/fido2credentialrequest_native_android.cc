// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "components/webauthn/android/jni_headers/Fido2CredentialRequest_jni.h"
#include "components/webauthn/json/value_conversions.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace webauthn {
namespace {

// MojoClassToJSON takes a serialized Mojo object and returns a Java String
// containing its JSON representation.
template <typename MojoClass>
static base::android::ScopedJavaLocalRef<jstring> MojoClassToJSON(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& byte_buffer) {
  const size_t serialized_len =
      base::checked_cast<size_t>(env->GetDirectBufferCapacity(byte_buffer));
  const uint8_t* const serialized =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(byte_buffer));
  auto options = MojoClass::New();
  CHECK(MojoClass::Deserialize(serialized, serialized_len, &options));
  base::Value value = webauthn::ToValue(options);
  std::string json;
  base::JSONWriter::Write(value, &json);
  return base::android::ConvertUTF8ToJavaString(env, json);
}

// MojoClassFromJSON takes a Java String, parses it as JSON, and then parses
// that with `parse_func` to produce a Mojo object whose serialisation is
// returned as a Java byte[].
template <typename MojoClass, typename ParseFuncType>
static base::android::ScopedJavaLocalRef<jbyteArray> MojoClassFromJSON(
    JNIEnv* env,
    ParseFuncType parse_func,
    const base::android::JavaParamRef<jstring>& jjson) {
  const std::string json = base::android::ConvertJavaStringToUTF8(env, jjson);
  const absl::optional<base::Value> parsed =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  if (!parsed) {
    LOG(ERROR) << __func__ << " failed to parse JSON";
    return nullptr;
  }
  const auto pair = parse_func(*parsed, webauthn::JSONUser::kAndroid);
  if (!pair.first) {
    LOG(ERROR) << __func__ << " failed to convert JSON: " << pair.second;
    return nullptr;
  }
  return base::android::ToJavaByteArray(env, MojoClass::Serialize(&pair.first));
}

}  // namespace

static base::android::ScopedJavaLocalRef<jstring>
JNI_Fido2CredentialRequest_CreateOptionsToJson(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& byte_buffer) {
  return MojoClassToJSON<blink::mojom::PublicKeyCredentialCreationOptions>(
      env, byte_buffer);
}

static base::android::ScopedJavaLocalRef<jbyteArray>
JNI_Fido2CredentialRequest_MakeCredentialResponseFromJson(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jjson) {
  return MojoClassFromJSON<blink::mojom::MakeCredentialAuthenticatorResponse>(
      env, webauthn::MakeCredentialResponseFromValue, jjson);
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_Fido2CredentialRequest_GetOptionsToJson(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& byte_buffer) {
  return MojoClassToJSON<blink::mojom::PublicKeyCredentialRequestOptions>(
      env, byte_buffer);
}

static base::android::ScopedJavaLocalRef<jbyteArray>
JNI_Fido2CredentialRequest_GetCredentialResponseFromJson(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jjson) {
  return MojoClassFromJSON<blink::mojom::GetAssertionAuthenticatorResponse>(
      env, webauthn::GetAssertionResponseFromValue, jjson);
}

}  // namespace webauthn
