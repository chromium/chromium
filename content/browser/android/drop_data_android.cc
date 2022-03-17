// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/drop_data_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

#include "ui/android/ui_android_jni_headers/DropDataAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace content {

// This function refers to //content/browser/renderer_host/data_transfer_util.cc
// on how data fields are populated. If data_transfer_util.cc ever changed, this
// function will need updates to keep in sync.
ScopedJavaLocalRef<jobject> ToJavaDropData(const DropData& drop_data) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jtext;
  if (drop_data.text) {
    jtext = ConvertUTF16ToJavaString(env, *drop_data.text);
  }

  // TODO(https://crbug.com/1289393): Revisit if drop_data.url is the correct
  // link for image with links.
  ScopedJavaLocalRef<jobject> jgurl;
  if (!drop_data.url.is_empty()) {
    jgurl = url::GURLAndroid::FromNativeGURL(env, drop_data.url);
    jtext = ConvertUTF16ToJavaString(env, drop_data.url_title);
  }

  // If file_contents is not empty, user is trying to drag image out of the
  // web contents.
  ScopedJavaLocalRef<jbyteArray> jimage_bytes;
  ScopedJavaLocalRef<jstring> jimage_extension;
  if (!drop_data.file_contents.empty()) {
    jimage_bytes = ToJavaByteArray(env, drop_data.file_contents);
    jimage_extension = ConvertUTF8ToJavaString(
        env, drop_data.file_contents_filename_extension);
  }

  return ui::Java_DropDataAndroid_create(env, jtext, jgurl, jimage_bytes,
                                         jimage_extension);
}  // ToJavaDropData

}  // namespace content
