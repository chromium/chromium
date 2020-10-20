// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_provider_gms_user_consent.h"

#include <string>

#include "base/bind.h"
#include "url/gurl.h"
#include "url/origin.h"

#include "content/public/android/content_jni_headers/SmsUserConsentReceiver_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

namespace content {

SmsProviderGmsUserConsent::SmsProviderGmsUserConsent() : SmsProvider() {
  // This class is constructed a single time whenever the
  // first web page uses the SMS Retriever API to wait for
  // SMSes.
  JNIEnv* env = AttachCurrentThread();
  j_sms_receiver_.Reset(Java_SmsUserConsentReceiver_create(
      env, reinterpret_cast<intptr_t>(this)));
}

SmsProviderGmsUserConsent::~SmsProviderGmsUserConsent() {
  JNIEnv* env = AttachCurrentThread();
  Java_SmsUserConsentReceiver_destroy(env, j_sms_receiver_);
}

void SmsProviderGmsUserConsent::Retrieve(RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents || !web_contents->GetTopLevelNativeWindow())
    return;

  JNIEnv* env = AttachCurrentThread();
  Java_SmsUserConsentReceiver_listen(
      env, j_sms_receiver_,
      web_contents->GetTopLevelNativeWindow()->GetJavaObject());
}

void SmsProviderGmsUserConsent::OnReceive(JNIEnv* env, jstring message) {
  std::string sms = ConvertJavaStringToUTF8(env, message);
  NotifyReceive(sms);
}

void SmsProviderGmsUserConsent::OnTimeout(JNIEnv* env) {
  NotifyFailure(SmsFetcher::FailureType::kPromptTimeout);
}

void SmsProviderGmsUserConsent::OnCancel(JNIEnv* env) {
  NotifyFailure(SmsFetcher::FailureType::kPromptCancelled);
}

base::android::ScopedJavaGlobalRef<jobject>
SmsProviderGmsUserConsent::GetWebOTPServiceForTesting() const {
  return j_sms_receiver_;
}

}  // namespace content
