// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "net/base/features.h"
#include "net/cert/cert_database.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "net/net_jni_headers/X509Util_jni.h"

using jni_zero::JavaRef;

namespace net {

static void JNI_X509Util_NotifyTrustStoreChanged(JNIEnv* env) {
  CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
}

static void JNI_X509Util_NotifyClientCertStoreChanged(JNIEnv* env) {
  CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
}

static bool JNI_X509Util_UseLockFreeVerification(JNIEnv* env) {
  return base::FeatureList::IsEnabled(
      net::features::kUseLockFreeX509Verification);
}

}  // namespace net

DEFINE_JNI(X509Util)
