// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_MDOCS_MDOC_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_WEBID_MDOCS_MDOC_PROVIDER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "content/browser/webid/mdocs/mdoc_provider.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

#include <jni.h>
#include <string>

using blink::mojom::WalletFieldRequirementPtr;

namespace content {

class WebContents;

// Android specific implementation of `MDocProvider`. It communicates with
// native apps via JNI. Once an mdoc is returned from Android apps, it sends it
// back to the browser where the API is initiated.
class CONTENT_EXPORT MDocProviderAndroid : public MDocProvider {
 public:
  MDocProviderAndroid();
  ~MDocProviderAndroid() override;

  MDocProviderAndroid(const MDocProviderAndroid&) = delete;
  MDocProviderAndroid& operator=(const MDocProviderAndroid&) = delete;

  using MDocCallback = base::OnceCallback<void(std::string)>;
  // Implementation of corresponding JNI methods in
  // MDocProviderAndroid.Natives.*
  void OnReceive(JNIEnv*, jstring mdoc);
  void OnError(JNIEnv*);

  // Triggers a mdoc request intent.
  void RequestMDoc(WebContents* web_contents,
                   const url::Origin& origin,
                   const base::Value::Dict& request,
                   MDocCallback callback) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_mdoc_provider_android_;
  MDocCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_MDOCS_MDOC_PROVIDER_ANDROID_H_
