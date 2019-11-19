// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/android/interstitial_page_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/android/content_test_jni/InterstitialPageDelegateAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

InterstitialPageDelegateAndroid::InterstitialPageDelegateAndroid(
    JNIEnv* env,
    jobject obj,
    const std::string& html_content)
    : weak_java_obj_(env, obj), html_content_(html_content), page_(NULL) {}

InterstitialPageDelegateAndroid::~InterstitialPageDelegateAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_obj_.get(env);
  if (obj.obj())
    Java_InterstitialPageDelegateAndroid_onNativeDestroyed(env, obj);
}

void InterstitialPageDelegateAndroid::Proceed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (page_)
    page_->Proceed();
}

void InterstitialPageDelegateAndroid::DontProceed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (page_)
    page_->DontProceed();
}

void InterstitialPageDelegateAndroid::ShowInterstitialPage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  InterstitialPage* interstitial =
      InterstitialPage::Create(web_contents, false, url, this);
  set_interstitial_page(interstitial);
  interstitial->Show();
}

std::string InterstitialPageDelegateAndroid::GetHTMLContents() {
  return html_content_;
}

void InterstitialPageDelegateAndroid::OnProceed() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_obj_.get(env);
  if (obj.obj())
    Java_InterstitialPageDelegateAndroid_onProceed(env, obj);
}

void InterstitialPageDelegateAndroid::OnDontProceed() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_obj_.get(env);
  if (obj.obj())
    Java_InterstitialPageDelegateAndroid_onDontProceed(env, obj);
}

void InterstitialPageDelegateAndroid::CommandReceived(
    const std::string& command) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_obj_.get(env);
  if (obj.obj()) {
    std::string sanitized_command(command);
    // The JSONified response has quotes, remove them.
    if (sanitized_command.length() > 1 && sanitized_command[0] == '"') {
      sanitized_command =
          sanitized_command.substr(1, sanitized_command.length() - 2);
    }

    Java_InterstitialPageDelegateAndroid_commandReceived(
        env, obj,
        base::android::ConvertUTF8ToJavaString(env, sanitized_command));
  }
}

static jlong JNI_InterstitialPageDelegateAndroid_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& html_content) {
  InterstitialPageDelegateAndroid* delegate =
      new InterstitialPageDelegateAndroid(
          env, obj, base::android::ConvertJavaStringToUTF8(env, html_content));
  return reinterpret_cast<intptr_t>(delegate);
}

}  // namespace content
