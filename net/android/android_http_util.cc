// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "net/http/http_util.h"
#include "net/net_jni_headers/HttpUtil_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertJavaStringToUTF16;

namespace net {

jboolean JNI_HttpUtil_IsAllowedHeader(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_header_name,
    const JavaParamRef<jstring>& j_header_value) {
  std::string header_name(ConvertJavaStringToUTF8(env, j_header_name));
  std::string header_value(ConvertJavaStringToUTF8(env, j_header_value));

  return HttpUtil::IsValidHeaderName(header_name)
    && HttpUtil::IsSafeHeader(header_name)
    && HttpUtil::IsValidHeaderValue(header_value);
}

}  // namespace net
