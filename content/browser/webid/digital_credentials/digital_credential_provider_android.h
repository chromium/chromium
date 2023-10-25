// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIAL_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIAL_PROVIDER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "content/browser/webid/digital_credentials/digital_credential_provider.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

#include <jni.h>
#include <string>

using blink::mojom::DigitalCredentialFieldRequirementPtr;

namespace content {

class WebContents;

// Android specific implementation of `DigitalCredentialProvider`. It
// communicates with native apps via JNI. Once an vc is returned from Android
// apps, it sends it back to the browser where the API is initiated.
class CONTENT_EXPORT DigitalCredentialProviderAndroid
    : public DigitalCredentialProvider {
 public:
  DigitalCredentialProviderAndroid();
  ~DigitalCredentialProviderAndroid() override;

  DigitalCredentialProviderAndroid(const DigitalCredentialProviderAndroid&) =
      delete;
  DigitalCredentialProviderAndroid& operator=(
      const DigitalCredentialProviderAndroid&) = delete;

  using DigitalCredentialCallback = base::OnceCallback<void(std::string)>;
  // Implementation of corresponding JNI methods in
  // DigitalCredentialProviderAndroid.Natives.*
  void OnReceive(JNIEnv*, jstring vc);
  void OnError(JNIEnv*);

  // Triggers a request for a digital credential.
  void RequestDigitalCredential(WebContents* web_contents,
                                const url::Origin& origin,
                                const base::Value::Dict& request,
                                DigitalCredentialCallback callback) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      j_digital_credential_provider_android_;
  DigitalCredentialCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_CREDENTIAL_PROVIDER_ANDROID_H_
