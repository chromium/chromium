// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_POLICY_ENFORCER_H_
#define NET_CERT_CT_POLICY_ENFORCER_H_

#include <stddef.h>

#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net {

class NetLogWithSource;

namespace ct {
enum class CTPolicyCompliance;
}  // namespace ct

class X509Certificate;

// Interface for checking whether or not a given certificate conforms to any
// policies an application may have regarding Certificate Transparency.
//
// See //net/docs/certificate-transparency.md for more details regarding the
// usage of CT in //net and risks that may exist when defining a CT policy.
class NET_EXPORT CTPolicyEnforcer {
 public:
  virtual ~CTPolicyEnforcer() = default;

  // Returns the CT certificate policy compliance status for a given
  // certificate and collection of SCTs.
  // |cert| is the certificate for which to check compliance, and
  // ||verified_scts| contains any/all SCTs associated with |cert| that
  // |have been verified (well-formed, issued by known logs, and
  // |applying to |cert|).
  virtual ct::CTPolicyCompliance CheckCompliance(
      X509Certificate* cert,
      const ct::SCTList& verified_scts,
      const NetLogWithSource& net_log) = 0;
};

// A default implementation of Certificate Transparency policies that is
// intended for use in applications without auto-update capabilities.
//
// See //net/docs/certificate-transparency.md for more details.
class NET_EXPORT DefaultCTPolicyEnforcer : public net::CTPolicyEnforcer {
 public:
  DefaultCTPolicyEnforcer() = default;
  ~DefaultCTPolicyEnforcer() override = default;

  ct::CTPolicyCompliance CheckCompliance(
      X509Certificate* cert,
      const ct::SCTList& verified_scts,
      const NetLogWithSource& net_log) override;
};

}  // namespace net

#endif  // NET_CERT_CT_POLICY_ENFORCER_H_
