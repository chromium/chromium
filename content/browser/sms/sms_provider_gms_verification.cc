// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_provider_gms_verification.h"

#include <string>

#include "base/bind.h"
#include "url/gurl.h"
#include "url/origin.h"

#include "content/public/android/content_jni_headers/SmsVerificationReceiver_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

namespace content {

SmsProviderGmsVerification::SmsProviderGmsVerification() {
  // This class is constructed a single time whenever the
  // first web page uses the SMS Retriever API to wait for
  // SMSes.
  JNIEnv* env = AttachCurrentThread();
  j_sms_receiver_.Reset(Java_SmsVerificationReceiver_create(
      env, reinterpret_cast<intptr_t>(this)));
}

SmsProviderGmsVerification::~SmsProviderGmsVerification() {
  JNIEnv* env = AttachCurrentThread();
  Java_SmsVerificationReceiver_destroy(env, j_sms_receiver_);
}

void SmsProviderGmsVerification::Retrieve(RenderFrameHost* render_frame_host) {
  JNIEnv* env = AttachCurrentThread();
  Java_SmsVerificationReceiver_listen(env, j_sms_receiver_);
}

void SmsProviderGmsVerification::OnReceive(JNIEnv* env, jstring message) {
  std::string sms = ConvertJavaStringToUTF8(env, message);
  NotifyReceive(sms);
}

void SmsProviderGmsVerification::OnTimeout(JNIEnv* env) {}

base::android::ScopedJavaGlobalRef<jobject>
SmsProviderGmsVerification::GetWebOTPServiceForTesting() const {
  return j_sms_receiver_;
}

}  // namespace content
