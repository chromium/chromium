// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/mdocs/mdoc_provider_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/android/content_jni_headers/MDocProviderAndroid_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace content {

MDocProviderAndroid::MDocProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  j_mdoc_provider_android_.Reset(
      Java_MDocProviderAndroid_create(env, reinterpret_cast<intptr_t>(this)));
}

MDocProviderAndroid::~MDocProviderAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_MDocProviderAndroid_destroy(env, j_mdoc_provider_android_);
}

void MDocProviderAndroid::RequestMDoc(
    WebContents* web_contents,
    const std::string& reader_public_key,
    const std::string& document_type,
    const std::vector<MDocElementPtr>& requested_elements,
    MDocCallback callback) {
  callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_reader_public_key =
      ConvertUTF8ToJavaString(env, reader_public_key);
  ScopedJavaLocalRef<jstring> j_document_type =
      ConvertUTF8ToJavaString(env, document_type);
  // TODO(crbug.com/1416939): pass `requested_elements` as |jobjectArray|.
  ScopedJavaLocalRef<jstring> j_requested_elements_namespace =
      ConvertUTF8ToJavaString(env, requested_elements[0]->element_namespace);
  ScopedJavaLocalRef<jstring> j_requested_elements_name =
      ConvertUTF8ToJavaString(env, requested_elements[0]->name);

  base::android::ScopedJavaLocalRef<jobject> j_window = nullptr;

  if (web_contents && web_contents->GetTopLevelNativeWindow()) {
    j_window = web_contents->GetTopLevelNativeWindow()->GetJavaObject();
  }

  Java_MDocProviderAndroid_requestMDoc(env, j_mdoc_provider_android_, j_window,
                                       j_reader_public_key, j_document_type,
                                       j_requested_elements_namespace,
                                       j_requested_elements_name);
}

void MDocProviderAndroid::OnReceive(JNIEnv* env, jstring j_mdoc) {
  std::string mdoc = ConvertJavaStringToUTF8(env, j_mdoc);
  if (callback_) {
    std::move(callback_).Run(mdoc);
  }
}

void MDocProviderAndroid::OnError(JNIEnv* env) {
  if (callback_) {
    std::move(callback_).Run("");
  }
}

}  // namespace content
