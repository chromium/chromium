// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_EV_ROOT_CA_METADATA_H_
#define NET_CERT_EV_ROOT_CA_METADATA_H_

#include "build/build_config.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "crypto/crypto_buildflags.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

#if BUILDFLAG(USE_NSS_CERTS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_FUCHSIA)
// When not defined, the EVRootCAMetadata singleton is a dumb placeholder
// implementation that will fail all EV lookup operations.
#define PLATFORM_USES_CHROMIUM_EV_METADATA
#endif

namespace base {
template <typename T>
struct LazyInstanceTraitsBase;
}  // namespace base

namespace net {

namespace der {
class Input;
}  // namespace der

// A singleton.  This class stores the meta data of the root CAs that issue
// extended-validation (EV) certificates.
class NET_EXPORT_PRIVATE EVRootCAMetadata {
 public:
#if BUILDFLAG(IS_WIN)
  typedef const char* PolicyOID;
#else
  // DER-encoded OID value (no tag or length).
  typedef der::Input PolicyOID;
#endif

  static EVRootCAMetadata* GetInstance();

  EVRootCAMetadata(const EVRootCAMetadata&) = delete;
  EVRootCAMetadata& operator=(const EVRootCAMetadata&) = delete;

  // Returns true if policy_oid is an EV policy OID of some root CA.
  bool IsEVPolicyOID(PolicyOID policy_oid) const;

  // Same as above but using the the DER-encoded OID (no tag or length).
  bool IsEVPolicyOIDGivenBytes(const der::Input& policy_oid) const;

  // Returns true if the root CA with the given certificate fingerprint has
  // the EV policy OID policy_oid.
  bool HasEVPolicyOID(const SHA256HashValue& fingerprint,
                      PolicyOID policy_oid) const;

  // Same as above but using the the DER-encoded OID (no tag or length).
  bool HasEVPolicyOIDGivenBytes(const SHA256HashValue& fingerprint,
                                const der::Input& policy_oid) const;

#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
  // Returns true if |policy_oid| is for 2.23.140.1.1 (CA/Browser Forum's
  // Extended Validation Policy). This is used as a hack by the
  // platform-specific CertVerifyProcs when doing EV verification.
  static bool IsCaBrowserForumEvOid(PolicyOID policy_oid);
#endif

  // AddEVCA adds an EV CA to the list of known EV CAs with the given policy.
  // |policy| is expressed as a string of dotted numbers. It returns true on
  // success.
  bool AddEVCA(const SHA256HashValue& fingerprint, const char* policy);

  // RemoveEVCA removes an EV CA that was previously added by AddEVCA. It
  // returns true on success.
  bool RemoveEVCA(const SHA256HashValue& fingerprint);

 private:
  friend struct base::LazyInstanceTraitsBase<EVRootCAMetadata>;

  EVRootCAMetadata();
  ~EVRootCAMetadata();

#if BUILDFLAG(IS_WIN)
  using ExtraEVCAMap = std::map<SHA256HashValue, std::string>;

  // extra_cas_ contains any EV CA metadata that was added at runtime.
  ExtraEVCAMap extra_cas_;
#elif defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
  using PolicyOIDMap = std::map<SHA256HashValue, std::vector<std::string>>;

  PolicyOIDMap ev_policy_;
  std::set<std::string> policy_oids_;
#endif
};

}  // namespace net

#endif  // NET_CERT_EV_ROOT_CA_METADATA_H_
