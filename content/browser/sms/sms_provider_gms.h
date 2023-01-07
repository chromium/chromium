// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_GMS_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_GMS_H_

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/sms/sms_provider.h"
#include "content/common/content_export.h"

namespace content {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser.sms
enum class GmsBackend : int {
  kAuto = 0,
  kUserConsent = 1,
  kVerification = 2,
};

class RenderFrameHost;

class CONTENT_EXPORT SmsProviderGms : public SmsProvider {
 public:
  SmsProviderGms();

  SmsProviderGms(const SmsProviderGms&) = delete;
  SmsProviderGms& operator=(const SmsProviderGms&) = delete;

  ~SmsProviderGms() override;

  void Retrieve(RenderFrameHost* rfh, SmsFetchType fetch_type) override;

  // Implementation of corresponding JNI methods in SmsProviderGms.Natives.*

  void OnReceive(JNIEnv*, jstring message, jint backend);
  void OnTimeout(JNIEnv* env);
  void OnCancel(JNIEnv* env);
  void OnNotAvailable(JNIEnv* env);

  void SetClientAndWindowForTesting(
      const base::android::JavaRef<jobject>& j_fake_client,
      const base::android::JavaRef<jobject>& j_window);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_sms_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_GMS_H_
