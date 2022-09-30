// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ev_root_ca_metadata.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <stdlib.h>
#endif

#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "net/der/input.h"
#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#endif

namespace net {

namespace {
#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
// Raw metadata.
struct EVMetadata {
  // kMaxOIDsPerCA is the number of OIDs that we can support per root CA. At
  // least one CA has different EV policies for business vs government
  // entities and, in the case of cross-signing, we might need to list another
  // CA's policy OID under the cross-signing root.
  static const size_t kMaxOIDsPerCA = 2;

  // The SHA-256 fingerprint of the root CA certificate, used as a unique
  // identifier for a root CA certificate.
  SHA256HashValue fingerprint;

  // The EV policy OIDs of the root CA.
  const base::StringPiece policy_oids[kMaxOIDsPerCA];
};

#include "net/data/ssl/chrome_root_store/chrome-ev-roots-inc.cc"

#endif  // defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
}  // namespace

static base::LazyInstance<EVRootCAMetadata>::Leaky g_ev_root_ca_metadata =
    LAZY_INSTANCE_INITIALIZER;

// static
EVRootCAMetadata* EVRootCAMetadata::GetInstance() {
  return g_ev_root_ca_metadata.Pointer();
}

#if BUILDFLAG(IS_WIN)

namespace {

bool ConvertBytesToDottedString(const der::Input& policy_oid,
                                std::string* dotted) {
  CBS cbs;
  CBS_init(&cbs, policy_oid.UnsafeData(), policy_oid.Length());
  bssl::UniquePtr<char> text(CBS_asn1_oid_to_text(&cbs));
  if (!text)
    return false;
  dotted->assign(text.get());
  return true;
}

}  // namespace

bool EVRootCAMetadata::IsEVPolicyOID(PolicyOID policy_oid) const {
  for (const auto& ev_root : kEvRootCaMetadata) {
    if (base::Contains(ev_root.policy_oids, policy_oid)) {
      return true;
    }
  }

  for (const auto& ca : extra_cas_) {
    if (ca.second == policy_oid)
      return true;
  }

  return false;
}

bool EVRootCAMetadata::IsEVPolicyOIDGivenBytes(
    const der::Input& policy_oid) const {
  std::string dotted;
  return ConvertBytesToDottedString(policy_oid, &dotted) &&
         IsEVPolicyOID(dotted.c_str());
}

bool EVRootCAMetadata::HasEVPolicyOID(const SHA256HashValue& fingerprint,
                                      PolicyOID policy_oid) const {
  for (const auto& ev_root : kEvRootCaMetadata) {
    if (fingerprint != ev_root.fingerprint)
      continue;
    return base::Contains(ev_root.policy_oids, policy_oid);
  }

  auto it = extra_cas_.find(fingerprint);
  return it != extra_cas_.end() && it->second == policy_oid;
}

bool EVRootCAMetadata::HasEVPolicyOIDGivenBytes(
    const SHA256HashValue& fingerprint,
    const der::Input& policy_oid) const {
  std::string dotted;
  return ConvertBytesToDottedString(policy_oid, &dotted) &&
         HasEVPolicyOID(fingerprint, dotted.c_str());
}

// static
bool EVRootCAMetadata::IsCaBrowserForumEvOid(PolicyOID policy_oid) {
  return strcmp(policy_oid, "2.23.140.1.1") == 0;
}

bool EVRootCAMetadata::AddEVCA(const SHA256HashValue& fingerprint,
                               const char* policy) {
  for (const auto& ev_root : kEvRootCaMetadata) {
    if (fingerprint == ev_root.fingerprint)
      return false;
  }
  if (extra_cas_.find(fingerprint) != extra_cas_.end())
    return false;

  extra_cas_[fingerprint] = policy;
  return true;
}

bool EVRootCAMetadata::RemoveEVCA(const SHA256HashValue& fingerprint) {
  auto it = extra_cas_.find(fingerprint);
  if (it == extra_cas_.end())
    return false;

  extra_cas_.erase(it);
  return true;
}

#elif defined(PLATFORM_USES_CHROMIUM_EV_METADATA)

namespace {

std::string OIDStringToDER(base::StringPiece policy) {
  uint8_t* der;
  size_t len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 32) ||
      !CBB_add_asn1_oid_from_text(cbb.get(), policy.data(), policy.size()) ||
      !CBB_finish(cbb.get(), &der, &len)) {
    return std::string();
  }
  bssl::UniquePtr<uint8_t> delete_der(der);
  return std::string(reinterpret_cast<const char*>(der), len);
}

}  // namespace

bool EVRootCAMetadata::IsEVPolicyOID(PolicyOID policy_oid) const {
  return policy_oids_.find(policy_oid.AsString()) != policy_oids_.end();
}

bool EVRootCAMetadata::IsEVPolicyOIDGivenBytes(
    const der::Input& policy_oid) const {
  // This implementation uses DER bytes already, so the two functions are the
  // same.
  return IsEVPolicyOID(policy_oid);
}

bool EVRootCAMetadata::HasEVPolicyOID(const SHA256HashValue& fingerprint,
                                      PolicyOID policy_oid) const {
  PolicyOIDMap::const_iterator iter = ev_policy_.find(fingerprint);
  if (iter == ev_policy_.end())
    return false;
  for (const std::string& ev_oid : iter->second) {
    if (der::Input(&ev_oid) == policy_oid)
      return true;
  }
  return false;
}

bool EVRootCAMetadata::HasEVPolicyOIDGivenBytes(
    const SHA256HashValue& fingerprint,
    const der::Input& policy_oid) const {
  // The implementation uses DER bytes already, so the two functions are the
  // same.
  return HasEVPolicyOID(fingerprint, policy_oid);
}

// static
bool EVRootCAMetadata::IsCaBrowserForumEvOid(PolicyOID policy_oid) {
  const uint8_t kCabEvOid[] = {0x67, 0x81, 0x0c, 0x01, 0x01};
  return der::Input(kCabEvOid) == policy_oid;
}

bool EVRootCAMetadata::AddEVCA(const SHA256HashValue& fingerprint,
                               const char* policy) {
  if (ev_policy_.find(fingerprint) != ev_policy_.end())
    return false;

  std::string der_policy = OIDStringToDER(policy);
  if (der_policy.empty())
    return false;

  ev_policy_[fingerprint].push_back(der_policy);
  policy_oids_.insert(der_policy);
  return true;
}

bool EVRootCAMetadata::RemoveEVCA(const SHA256HashValue& fingerprint) {
  PolicyOIDMap::iterator it = ev_policy_.find(fingerprint);
  if (it == ev_policy_.end())
    return false;
  std::string oid = it->second[0];
  ev_policy_.erase(it);
  policy_oids_.erase(oid);
  return true;
}

#else

// These are just stub functions for platforms where we don't use this EV
// metadata.
//

bool EVRootCAMetadata::IsEVPolicyOID(PolicyOID policy_oid) const {
  LOG(WARNING) << "Not implemented";
  return false;
}

bool EVRootCAMetadata::IsEVPolicyOIDGivenBytes(
    const der::Input& policy_oid) const {
  LOG(WARNING) << "Not implemented";
  return false;
}

bool EVRootCAMetadata::HasEVPolicyOID(const SHA256HashValue& fingerprint,
                                      PolicyOID policy_oid) const {
  LOG(WARNING) << "Not implemented";
  return false;
}

bool EVRootCAMetadata::HasEVPolicyOIDGivenBytes(
    const SHA256HashValue& fingerprint,
    const der::Input& policy_oid) const {
  LOG(WARNING) << "Not implemented";
  return false;
}

bool EVRootCAMetadata::AddEVCA(const SHA256HashValue& fingerprint,
                               const char* policy) {
  LOG(WARNING) << "Not implemented";
  return true;
}

bool EVRootCAMetadata::RemoveEVCA(const SHA256HashValue& fingerprint) {
  LOG(WARNING) << "Not implemented";
  return true;
}

#endif

EVRootCAMetadata::EVRootCAMetadata() {
// Constructs the object from the raw metadata in kEvRootCaMetadata.
#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA) && !BUILDFLAG(IS_WIN)
  for (const auto& ev_root : kEvRootCaMetadata) {
    for (const auto& policy : ev_root.policy_oids) {
      if (policy.empty())
        break;

      std::string policy_der = OIDStringToDER(policy.data());
      if (policy_der.empty()) {
        LOG(ERROR) << "Failed to register OID: " << policy;
        continue;
      }

      ev_policy_[ev_root.fingerprint].push_back(policy_der);
      policy_oids_.insert(policy_der);
    }
  }
#endif
}

EVRootCAMetadata::~EVRootCAMetadata() = default;

}  // namespace net
