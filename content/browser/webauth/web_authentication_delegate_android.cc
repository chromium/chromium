// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

// This "header" actually contain several function definitions and thus can
// only be included once across Chromium.
#include "content/public/android/content_jni_headers/WebAuthenticationDelegate_jni.h"

// These are JNI implementations of function used by the Java class
// `WebAuthenticationDelegate`.

static jlong JNI_WebAuthenticationDelegate_GetNativeDelegate(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(
      content::GetContentClient()->browser()->GetWebAuthenticationDelegate());
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_WebAuthenticationDelegate_GetIntentSender(
    JNIEnv* env,
    jlong delegatePtr,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* const web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  static_assert(sizeof(delegatePtr) >= sizeof(intptr_t));
  return reinterpret_cast<content::WebAuthenticationDelegate*>(
             static_cast<intptr_t>(delegatePtr))
      ->GetIntentSender(web_contents);
}

static int JNI_WebAuthenticationDelegate_GetSupportLevel(
    JNIEnv* env,
    jlong delegatePtr,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* const web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  static_assert(sizeof(delegatePtr) >= sizeof(intptr_t));
  return reinterpret_cast<content::WebAuthenticationDelegate*>(
             static_cast<intptr_t>(delegatePtr))
      ->GetSupportLevel(web_contents);
}
