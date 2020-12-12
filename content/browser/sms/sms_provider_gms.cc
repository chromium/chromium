// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_provider_gms.h"

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

#include "content/public/android/content_jni_headers/SmsProviderGms_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

namespace content {

SmsProviderGms::SmsProviderGms() {
  // The backend depends on |features::kWebOtpBackendAuto| which is controlled
  // by both a Finch experiment and chrome://flags. If the feature is enabled,
  // the "Auto" backend will be used. If not, for backward compatibility we use
  // the "UserConsent" backend. Note: in case of any conflict between finch and
  // chrome flags, the latter takes precedence.
  GmsBackend backend =
      base::FeatureList::IsEnabled(features::kWebOtpBackendAuto)
          ? GmsBackend::kAuto
          : GmsBackend::kUserConsent;

  auto switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebOtpBackend);
  if (switch_value == switches::kWebOtpBackendUserConsent) {
    backend = GmsBackend::kUserConsent;
  } else if (switch_value == switches::kWebOtpBackendSmsVerification) {
    backend = GmsBackend::kVerification;
  } else if (switch_value == switches::kWebOtpBackendAuto) {
    backend = GmsBackend::kAuto;
  }

  // This class is constructed a single time whenever the first web page uses
  // the SMS Retriever API to wait for SMSes.
  JNIEnv* env = AttachCurrentThread();
  j_sms_provider_.Reset(Java_SmsProviderGms_create(
      env, reinterpret_cast<intptr_t>(this), static_cast<int>(backend)));
}

SmsProviderGms::~SmsProviderGms() {
  JNIEnv* env = AttachCurrentThread();
  Java_SmsProviderGms_destroy(env, j_sms_provider_);
}

void SmsProviderGms::Retrieve(RenderFrameHost* render_frame_host) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  base::android::ScopedJavaLocalRef<jobject> j_window = nullptr;

  if (web_contents && web_contents->GetTopLevelNativeWindow()) {
    j_window = web_contents->GetTopLevelNativeWindow()->GetJavaObject();
  }

  JNIEnv* env = AttachCurrentThread();
  Java_SmsProviderGms_listen(env, j_sms_provider_, j_window);
}

void SmsProviderGms::OnReceive(JNIEnv* env, jstring message, jint backend) {
  GmsBackend b = static_cast<GmsBackend>(backend);
  auto consent_requirement = UserConsent::kNotObtained;
  if (b == GmsBackend::kUserConsent)
    consent_requirement = UserConsent::kObtained;

  std::string sms = ConvertJavaStringToUTF8(env, message);
  NotifyReceive(sms, consent_requirement);
}

void SmsProviderGms::OnTimeout(JNIEnv* env) {
  NotifyFailure(SmsFetcher::FailureType::kPromptTimeout);
}

void SmsProviderGms::OnCancel(JNIEnv* env) {
  NotifyFailure(SmsFetcher::FailureType::kPromptCancelled);
}

void SmsProviderGms::OnNotAvailable(JNIEnv* env) {
  NotifyFailure(SmsFetcher::FailureType::kBackendNotAvailable);
}

void SmsProviderGms::SetClientAndWindowForTesting(
    const base::android::JavaRef<jobject>& j_fake_client,
    const base::android::JavaRef<jobject>& j_window) {
  JNIEnv* env = AttachCurrentThread();
  Java_SmsProviderGms_setClientAndWindow(env, j_sms_provider_, j_fake_client,
                                         j_window);
}

}  // namespace content
