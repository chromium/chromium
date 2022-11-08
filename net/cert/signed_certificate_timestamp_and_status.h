// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_AND_STATUS_H_
#define NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_AND_STATUS_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net {

struct NET_EXPORT SignedCertificateTimestampAndStatus {
  SignedCertificateTimestampAndStatus();

  SignedCertificateTimestampAndStatus(
      const scoped_refptr<ct::SignedCertificateTimestamp>& sct,
      ct::SCTVerifyStatus status);

  SignedCertificateTimestampAndStatus(
      const SignedCertificateTimestampAndStatus& other);

  ~SignedCertificateTimestampAndStatus();

  scoped_refptr<ct::SignedCertificateTimestamp> sct;
  ct::SCTVerifyStatus status;
};

typedef std::vector<SignedCertificateTimestampAndStatus>
    SignedCertificateTimestampAndStatusList;

}  // namespace net

#endif  // NET_CERT_SIGNED_CERTIFICATE_TIMESTAMP_AND_STATUS_H_
