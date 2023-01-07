// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_
#define NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"

namespace net::android {

// The list of certificate verification results returned from Java side to the
// C++ side.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum CertVerifyStatusAndroid {
  // Certificate is trusted.
  CERT_VERIFY_STATUS_ANDROID_OK = 0,
  // Certificate verification could not be conducted.
  CERT_VERIFY_STATUS_ANDROID_FAILED = -1,
  // Certificate is not trusted due to non-trusted root of the certificate
  // chain.
  CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT = -2,
  // Certificate is not trusted because it has expired.
  CERT_VERIFY_STATUS_ANDROID_EXPIRED = -3,
  // Certificate is not trusted because it is not valid yet.
  CERT_VERIFY_STATUS_ANDROID_NOT_YET_VALID = -4,
  // Certificate is not trusted because it could not be parsed.
  CERT_VERIFY_STATUS_ANDROID_UNABLE_TO_PARSE = -5,
  // Certificate is not trusted because it has an extendedKeyUsage field, but
  // its value is not correct for a web server.
  CERT_VERIFY_STATUS_ANDROID_INCORRECT_KEY_USAGE = -6,
};

// Extract parameters out of an AndroidCertVerifyResult object.
void ExtractCertVerifyResult(const base::android::JavaRef<jobject>& result,
                             CertVerifyStatusAndroid* status,
                             bool* is_issued_by_known_root,
                             std::vector<std::string>* verified_chain);

}  // namespace net::android

#endif  // NET_ANDROID_CERT_VERIFY_RESULT_ANDROID_H_
