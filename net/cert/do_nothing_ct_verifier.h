// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_DO_NOTHING_CT_VERIFIER_H_
#define NET_CERT_DO_NOTHING_CT_VERIFIER_H_

#include <string_view>

#include "net/base/net_export.h"
#include "net/cert/ct_verifier.h"

namespace net {

// An implementation of CTVerifier that does not validate SCTs.
//
// SECURITY NOTE:
// As Certificate Transparency is an essential part in safeguarding TLS
// connections, disabling Certificate Transparency enforcement is a decision
// that should not be taken lightly, and it should be made an explicit
// decision rather than a potentially accidental decision (such as allowing
// for a nullptr instance). By checking Certificate Transparency information,
// typically via a net::MultiLogCTVerifier, and enforcing policies related
// to Certificate Transparency provided by a net::CTPolicyEnforcer, developers
// can help protect their users by ensuring that misissued TLS certificates
// are detected.
//
// However, not every consumer of TLS certificates is using the Web PKI. For
// example, they may be using connections authenticated out of band, or may
// be using private or local PKIs for which Certificate Transparency is not
// relevant. Alternatively, much like how a robust and secure TLS client
// requires a regularly updated root certificate store, a robust and secure
// Certificate Transparency client requires regular updates. However, since
// some clients may not support regular updates, it may be intentional to
// disable Certificate Transparency and choose a less-secure default
// behavior.
//
// Consumers of this class should generally try to get a security or design
// to discuss the type of net::X509Certificates they will be validating,
// and determine whether or not Certificate Transparency is right for the
// particular use case.
//
// Because of the complex nuances related to security tradeoffs, it is
// expected that classes which expect a CTVerifier will require one to be
// supplied, forcing the caller to make an intentional and explicit decision
// about the appropriate security policy, rather than leaving it ambiguous,
// such as via a nullptr. This class is intended to indicate an intentional
// consideration of CT, and a decision to not support it.
class NET_EXPORT DoNothingCTVerifier : public CTVerifier {
 public:
  DoNothingCTVerifier();

  DoNothingCTVerifier(const DoNothingCTVerifier&) = delete;
  DoNothingCTVerifier& operator=(const DoNothingCTVerifier&) = delete;

  ~DoNothingCTVerifier() override;

  void Verify(X509Certificate* cert,
              std::string_view stapled_ocsp_response,
              std::string_view sct_list_from_tls_extension,
              base::Time current_time,
              SignedCertificateTimestampAndStatusList* output_scts,
              const NetLogWithSource& net_log) const override;
};

}  // namespace net

#endif  // NET_CERT_DO_NOTHING_CT_VERIFIER_H_
