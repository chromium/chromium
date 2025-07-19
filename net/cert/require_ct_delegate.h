// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_REQUIRE_CT_DELEGATE_H_
#define NET_CERT_REQUIRE_CT_DELEGATE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/ct_policy_status.h"

namespace net {

class X509Certificate;

class NET_EXPORT RequireCTDelegate
    : public base::RefCountedThreadSafe<RequireCTDelegate> {
 public:
  // Provides a capability for altering the default handling of Certificate
  // Transparency information, allowing it to be always required for some
  // hosts, for some hosts to be opted out of the default policy, or
  // allowing the TransportSecurityState to apply the default security
  // policies.
  enum class CTRequirementLevel {
    // The host is required to always supply Certificate Transparency
    // information that complies with the CT policy.
    REQUIRED,

    // The host is explicitly not required to supply Certificate
    // Transparency information that complies with the CT policy.
    NOT_REQUIRED,
  };

  // Called by the TransportSecurityState, allows the Delegate to override
  // the default handling of Certificate Transparency requirements, if
  // desired.
  // |hostname| contains the host being contacted, serving the certificate
  // |chain|, with the hashes |hashes| which must be in the same order as the
  // certificate chain (leaf to root).
  virtual CTRequirementLevel IsCTRequiredForHost(
      std::string_view hostname,
      const X509Certificate* chain,
      const std::vector<SHA256HashValue>& hashes) const = 0;

  // Returns CT_REQUIREMENTS_NOT_MET if a connection violates CT policy
  // requirements: that is, if a connection to |host|, using the validated
  // certificate |validated_certificate_chain|, is expected to be accompanied
  // with valid Certificate Transparency information that complies with the
  // connection's CTPolicyEnforcer and |policy_compliance| indicates that
  // the connection does not comply.
  //
  // |public_key_hashes| must be in the same order as the certificate chain
  // (leaf to root).
  //
  // If |delegate| is null, CT will not be required.
  static ct::CTRequirementsStatus CheckCTRequirements(
      const RequireCTDelegate* delegate,
      std::string_view host,
      bool is_issued_by_known_root,
      const std::vector<SHA256HashValue>& public_key_hashes,
      const X509Certificate* validated_certificate_chain,
      ct::CTPolicyCompliance policy_compliance);

 protected:
  virtual ~RequireCTDelegate() = default;

 private:
  friend class base::RefCountedThreadSafe<RequireCTDelegate>;
};

}  // namespace net

#endif  // NET_CERT_REQUIRE_CT_DELEGATE_H_
