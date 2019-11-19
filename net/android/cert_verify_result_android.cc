// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/cert_verify_result_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "net/net_jni_headers/AndroidCertVerifyResult_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaArrayOfByteArrayToStringVector;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace net {
namespace android {

void ExtractCertVerifyResult(const JavaRef<jobject>& result,
                             CertVerifyStatusAndroid* status,
                             bool* is_issued_by_known_root,
                             std::vector<std::string>* verified_chain) {
  JNIEnv* env = AttachCurrentThread();

  *status = static_cast<CertVerifyStatusAndroid>(
      Java_AndroidCertVerifyResult_getStatus(env, result));

  *is_issued_by_known_root =
      Java_AndroidCertVerifyResult_isIssuedByKnownRoot(env, result);

  ScopedJavaLocalRef<jobjectArray> chain_byte_array =
      Java_AndroidCertVerifyResult_getCertificateChainEncoded(env, result);
  JavaArrayOfByteArrayToStringVector(env, chain_byte_array, verified_chain);
}

}  // namespace android
}  // namespace net
