// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "components/webauthn/android/webauthn_mode.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webauthn/android/jni_headers/WebauthnModeProvider_jni.h"

namespace webauthn {

namespace {
const void* const kWebauthnModeUserDataKey = &kWebauthnModeUserDataKey;
}  // namespace

using base::android::JavaParamRef;
using content::WebContents;

class WebauthnModeWrapper : public base::SupportsUserData::Data {
 public:
  explicit WebauthnModeWrapper(WebauthnMode mode) : mode_(mode) {}
  ~WebauthnModeWrapper() override = default;

  static WebauthnModeWrapper* FromWebContents(
      content::WebContents* web_contents) {
    return static_cast<WebauthnModeWrapper*>(
        web_contents->GetUserData(kWebauthnModeUserDataKey));
  }

  WebauthnMode GetMode() { return mode_; }

 private:
  const WebauthnMode mode_;
};

// static
jlong JNI_WebauthnModeProvider_SetWebauthnModeForWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jint mode) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  WebauthnModeWrapper* obj = new WebauthnModeWrapper(WebauthnMode(mode));
  web_contents->SetUserData(kWebauthnModeUserDataKey, base::WrapUnique(obj));
  return reinterpret_cast<intptr_t>(obj);
}

// static
jint JNI_WebauthnModeProvider_GetWebauthnModeForWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  if (WebauthnModeWrapper* webauthnMode =
          WebauthnModeWrapper::FromWebContents(web_contents)) {
    return webauthnMode->GetMode();
  }
  return WebauthnMode::NONE;
}

}  // namespace webauthn
