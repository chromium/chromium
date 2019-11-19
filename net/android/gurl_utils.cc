// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "net/net_jni_headers/GURLUtils_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace net {

ScopedJavaLocalRef<jstring> JNI_GURLUtils_GetOrigin(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  GURL host(base::android::ConvertJavaStringToUTF16(env, url));

  return base::android::ConvertUTF8ToJavaString(env, host.GetOrigin().spec());
}

ScopedJavaLocalRef<jstring> JNI_GURLUtils_GetScheme(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  GURL host(base::android::ConvertJavaStringToUTF16(env, url));

  return base::android::ConvertUTF8ToJavaString(env, host.scheme());
}

}  // namespace net
