// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_identity_provider_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "content/public/android/content_jni_headers/DigitalIdentityProvider_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace content {

DigitalIdentityProviderAndroid::DigitalIdentityProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  j_digital_identity_provider_android_.Reset(
      Java_DigitalIdentityProvider_create(env,
                                            reinterpret_cast<intptr_t>(this)));
}

DigitalIdentityProviderAndroid::~DigitalIdentityProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_DigitalIdentityProvider_destroy(
      env, j_digital_identity_provider_android_);
}

void DigitalIdentityProviderAndroid::Request(WebContents* web_contents,
                                             const url::Origin& origin,
                                             const base::Value::Dict& request,
                                             DigitalIdentityCallback callback) {
  callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_origin =
      ConvertUTF8ToJavaString(env, origin.Serialize());
  std::string json =
      WriteJsonWithOptions(request, base::JSONWriter::OPTIONS_PRETTY_PRINT)
          .value();
  ScopedJavaLocalRef<jstring> j_request = ConvertUTF8ToJavaString(env, json);

  base::android::ScopedJavaLocalRef<jobject> j_window = nullptr;

  if (web_contents && web_contents->GetTopLevelNativeWindow()) {
    j_window = web_contents->GetTopLevelNativeWindow()->GetJavaObject();
  }

  Java_DigitalIdentityProvider_request(
      env, j_digital_identity_provider_android_, j_window, j_origin,
      j_request);
}

void DigitalIdentityProviderAndroid::OnReceive(JNIEnv* env,
                                               jstring j_digital_identity,
                                               jint status_for_metrics) {
  if (callback_) {
    std::string digital_identity =
        ConvertJavaStringToUTF8(env, j_digital_identity);
    std::move(callback_).Run(
        digital_identity,
        static_cast<DigitalIdentityProvider::RequestStatusForMetrics>(
            status_for_metrics));
  }
}

}  // namespace content
