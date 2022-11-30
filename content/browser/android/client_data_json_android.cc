// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "content/browser/webauth/client_data_json.h"
#include "content/public/android/content_jni_headers/ClientDataJsonImpl_jni.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace content {
namespace {

void DeserializePaymentOptionsFromJavaByteBuffer(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jbuffer,
    mojo::StructPtr<blink::mojom::PaymentOptions>* out) {
  DCHECK(out);
  if (jbuffer.is_null()) {
    *out = nullptr;
    return;
  }
  jbyte* buf_in =
      static_cast<jbyte*>(env->GetDirectBufferAddress(jbuffer.obj()));
  jlong buf_size = env->GetDirectBufferCapacity(jbuffer.obj());
  if (buf_size == 0) {
    *out = nullptr;
    return;
  }
  bool success =
      blink::mojom::PaymentOptions::Deserialize(buf_in, buf_size, out);
  DCHECK(success);
}

}  // namespace

static base::android::ScopedJavaLocalRef<jstring>
JNI_ClientDataJsonImpl_BuildClientDataJson(
    JNIEnv* env,
    jint jclient_data_request_type,
    const base::android::JavaParamRef<jstring>& jcaller_origin,
    const base::android::JavaParamRef<jbyteArray>& jchallenge,
    jboolean jis_cross_origin,
    const base::android::JavaParamRef<jobject>& joptions_byte_buffer,
    const base::android::JavaParamRef<jstring>& jrelying_party_id,
    const base::android::JavaParamRef<jstring>& jtop_origin) {
  ClientDataRequestType type =
      static_cast<ClientDataRequestType>(jclient_data_request_type);
  std::string caller_origin =
      base::android::ConvertJavaStringToUTF8(env, jcaller_origin);
  std::vector<uint8_t> challenge;
  base::android::JavaByteArrayToByteVector(env, jchallenge, &challenge);
  bool is_cross_origin = static_cast<bool>(jis_cross_origin);

  blink::mojom::PaymentOptionsPtr options;
  DeserializePaymentOptionsFromJavaByteBuffer(env, joptions_byte_buffer,
                                              &options);

  std::string relying_party_id =
      jrelying_party_id
          ? base::android::ConvertJavaStringToUTF8(env, jrelying_party_id)
          : "";
  std::string top_origin =
      jtop_origin ? base::android::ConvertJavaStringToUTF8(env, jtop_origin)
                  : "";

  ClientDataJsonParams client_data_json_params(
      /*type=*/type, /*origin=*/url::Origin::Create(GURL(caller_origin)),
      /*challenge=*/challenge, /*is_cross_origin_iframe=*/is_cross_origin);
  client_data_json_params.payment_options = std::move(options);
  client_data_json_params.payment_rp = relying_party_id;
  client_data_json_params.payment_top_origin = top_origin;
  std::string client_data_json =
      BuildClientDataJson(std::move(client_data_json_params));
  return base::android::ConvertUTF8ToJavaString(env, client_data_json);
}

}  // namespace content
