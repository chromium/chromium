// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_OCSP_REVOCATION_STATUS_H_
#define NET_CERT_OCSP_REVOCATION_STATUS_H_

namespace net {

// This value is histogrammed, so do not re-order or change values, and add
// new values at the end.
enum class OCSPRevocationStatus {
  GOOD = 0,
  REVOKED = 1,
  UNKNOWN = 2,
  MAX_VALUE = UNKNOWN
};

}  // namespace net

#endif  // NET_CERT_OCSP_REVOCATION_STATUS_H_
