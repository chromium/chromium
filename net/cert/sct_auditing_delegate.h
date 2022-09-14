// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_SCT_AUDITING_DELEGATE_H_
#define NET_CERT_SCT_AUDITING_DELEGATE_H_

#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"

namespace net {

class X509Certificate;

// An interface for controlling SCT auditing behavior.
class NET_EXPORT SCTAuditingDelegate {
 public:
  virtual ~SCTAuditingDelegate() = default;

  virtual void MaybeEnqueueReport(
      const net::HostPortPair& host_port_pair,
      const net::X509Certificate* validated_certificate_chain,
      const net::SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps) = 0;
};

}  // namespace net

#endif  // NET_CERT_SCT_AUDITING_DELEGATE_H_
