// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/digital_identity_provider.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom.h"
#include "url/origin.h"

#include <jni.h>

namespace content {

class WebContents;

// Android specific implementation of `DigitalIdentityProvider`. It
// communicates with native apps via JNI. Once an identity is returned from Android
// apps, it sends it back to the browser where the API is initiated.
class CONTENT_EXPORT DigitalIdentityProviderAndroid
    : public DigitalIdentityProvider {
 public:
  DigitalIdentityProviderAndroid();
  ~DigitalIdentityProviderAndroid() override;

  DigitalIdentityProviderAndroid(const DigitalIdentityProviderAndroid&) =
      delete;
  DigitalIdentityProviderAndroid& operator=(
      const DigitalIdentityProviderAndroid&) = delete;

  // Implementation of corresponding JNI methods in
  // DigitalIdentityProviderAndroid.Natives.*
  void OnReceive(JNIEnv*, jstring vc, jint status_for_metrics);

  // Triggers a request for a digital credential.
  void Request(WebContents* web_contents,
               const url::Origin& origin,
               const base::Value::Dict& request,
               DigitalIdentityCallback callback) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      j_digital_identity_provider_android_;
  DigitalIdentityCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_ANDROID_H_
