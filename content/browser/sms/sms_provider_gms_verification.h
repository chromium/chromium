// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_GMS_VERIFICATION_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_GMS_VERIFICATION_H_

#include <utility>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/browser/sms/sms_provider.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT SmsProviderGmsVerification : public SmsProvider {
 public:
  SmsProviderGmsVerification();
  ~SmsProviderGmsVerification() override;

  void Retrieve(RenderFrameHost* rfh) override;

  // Implements JNI method SmsVerificationReceiver.Natives.onReceive().
  void OnReceive(JNIEnv*, jstring message);

  // Implements JNI method SmsVerificationReceiver.Natives.onTimeout().
  void OnTimeout(JNIEnv* env);

  base::android::ScopedJavaGlobalRef<jobject> GetWebOTPServiceForTesting()
      const;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_sms_receiver_;

  DISALLOW_COPY_AND_ASSIGN(SmsProviderGmsVerification);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_GMS_VERIFICATION_H_
