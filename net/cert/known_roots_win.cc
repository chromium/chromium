// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/known_roots_win.h"

#include "base/metrics/histogram_macros.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_util_win.h"

namespace net {

bool IsKnownRoot(PCCERT_CONTEXT cert) {
  BYTE hash_prop[32] = {0};
  DWORD size = sizeof(hash_prop);
  return CertGetCertificateContextProperty(
             cert, CERT_AUTH_ROOT_SHA256_HASH_PROP_ID, &hash_prop, &size) &&
         size == sizeof(hash_prop);
}

}  // namespace net
