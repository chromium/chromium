// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_nss.h"

#include <cert.h>
#include <certdb.h>
#include <certt.h>
#include <pk11pub.h>
#include <pkcs11n.h>
#include <pkcs11t.h>
#include <seccomon.h>
#include <secmod.h>
#include <secmodt.h>

#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/cert/known_roots_nss.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/trust_store.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"

namespace net {

namespace {

const void* kResultDebugDataKey = &kResultDebugDataKey;

TrustStoreNSS::ResultDebugData::SlotFilterType GetSlotFilterType(
    const TrustStoreNSS::UserSlotTrustSetting& user_slot_trust_setting) {
  if (absl::holds_alternative<TrustStoreNSS::UseTrustFromAllUserSlots>(
          user_slot_trust_setting)) {
    return TrustStoreNSS::ResultDebugData::SlotFilterType::kDontFilter;
  }
  if (absl::get<crypto::ScopedPK11Slot>(user_slot_trust_setting) == nullptr) {
    return TrustStoreNSS::ResultDebugData::SlotFilterType::kDoNotAllowUserSlots;
  }
  return TrustStoreNSS::ResultDebugData::SlotFilterType::
      kAllowSpecifiedUserSlot;
}

struct FreePK11GenericObjects {
  void operator()(PK11GenericObject* x) const {
    if (x) {
      PK11_DestroyGenericObjects(x);
    }
  }
};
using ScopedPK11GenericObjects =
    std::unique_ptr<PK11GenericObject, FreePK11GenericObjects>;

// Get the list of all slots `nss_cert` is present in, along with the object
// handle of the cert in each of those slots.
//
// (Note that there is a PK11_GetAllSlotsForCert function that *seems* like it
// would be useful here, however it does not actually return all relevant
// slots.)
std::vector<std::pair<crypto::ScopedPK11Slot, CK_OBJECT_HANDLE>>
GetAllSlotsAndHandlesForCert(CERTCertificate* nss_cert) {
  std::vector<std::pair<crypto::ScopedPK11Slot, CK_OBJECT_HANDLE>> r;
  crypto::AutoSECMODListReadLock lock_id;
  for (const SECMODModuleList* item = SECMOD_GetDefaultModuleList();
       item != nullptr; item = item->next) {
    for (int i = 0; i < item->module->slotCount; ++i) {
      PK11SlotInfo* slot = item->module->slots[i];
      if (PK11_IsPresent(slot)) {
        CK_OBJECT_HANDLE handle = PK11_FindCertInSlot(slot, nss_cert, nullptr);
        if (handle != CK_INVALID_HANDLE) {
          r.emplace_back(PK11_ReferenceSlot(slot), handle);
        }
      }
    }
  }
  return r;
}

bool IsMozillaCaPolicyProvided(PK11SlotInfo* slot,
                               CK_OBJECT_HANDLE cert_handle) {
  return PK11_HasRootCerts(slot) &&
         PK11_HasAttributeSet(slot, cert_handle, CKA_NSS_MOZILLA_CA_POLICY,
                              /*haslock=*/PR_FALSE) == CK_TRUE;
}

bool IsCertOnlyInNSSRoots(CERTCertificate* cert) {
  std::vector<std::pair<crypto::ScopedPK11Slot, CK_OBJECT_HANDLE>>
      slots_and_handles_for_cert = GetAllSlotsAndHandlesForCert(cert);
  for (const auto& [slot, handle] : slots_and_handles_for_cert) {
    if (IsMozillaCaPolicyProvided(slot.get(), handle)) {
      // Cert is an NSS root. Continue looking to see if it also is present in
      // another slot.
      continue;
    }
    // Found cert in a non-NSS roots slot.
    return false;
  }
  // Cert was only found in NSS roots (or was not in any slots, but that
  // shouldn't happen.)
  return true;
}

}  // namespace

TrustStoreNSS::ResultDebugData::ResultDebugData(
    bool ignore_system_trust_settings,
    SlotFilterType slot_filter_type)
    : ignore_system_trust_settings_(ignore_system_trust_settings),
      slot_filter_type_(slot_filter_type) {}

// static
const TrustStoreNSS::ResultDebugData* TrustStoreNSS::ResultDebugData::Get(
    const base::SupportsUserData* debug_data) {
  return static_cast<ResultDebugData*>(
      debug_data->GetUserData(kResultDebugDataKey));
}

// static
void TrustStoreNSS::ResultDebugData::Create(
    bool ignore_system_trust_settings,
    SlotFilterType slot_filter_type,
    base::SupportsUserData* debug_data) {
  debug_data->SetUserData(kResultDebugDataKey,
                          std::make_unique<ResultDebugData>(
                              ignore_system_trust_settings, slot_filter_type));
}

std::unique_ptr<base::SupportsUserData::Data>
TrustStoreNSS::ResultDebugData::Clone() {
  return std::make_unique<ResultDebugData>(*this);
}

TrustStoreNSS::ListCertsResult::ListCertsResult(ScopedCERTCertificate cert,
                                                CertificateTrust trust)
    : cert(std::move(cert)), trust(trust) {}
TrustStoreNSS::ListCertsResult::~ListCertsResult() = default;

TrustStoreNSS::ListCertsResult::ListCertsResult(ListCertsResult&& other) =
    default;
TrustStoreNSS::ListCertsResult& TrustStoreNSS::ListCertsResult::operator=(
    ListCertsResult&& other) = default;

TrustStoreNSS::TrustStoreNSS(SystemTrustSetting system_trust_setting,
                             UserSlotTrustSetting user_slot_trust_setting)
    : ignore_system_trust_settings_(system_trust_setting == kIgnoreSystemTrust),
      user_slot_trust_setting_(std::move(user_slot_trust_setting)) {}

TrustStoreNSS::~TrustStoreNSS() = default;

void TrustStoreNSS::SyncGetIssuersOf(const ParsedCertificate* cert,
                                     ParsedCertificateList* issuers) {
  crypto::EnsureNSSInit();

  SECItem name;
  // Use the original issuer value instead of the normalized version. NSS does a
  // less extensive normalization in its Name comparisons, so our normalized
  // version may not match the unnormalized version.
  name.len = cert->tbs().issuer_tlv.Length();
  name.data = const_cast<uint8_t*>(cert->tbs().issuer_tlv.UnsafeData());
  // |validOnly| in CERT_CreateSubjectCertList controls whether to return only
  // certs that are valid at |sorttime|. Expiration isn't meaningful for trust
  // anchors, so request all the matches.
  crypto::ScopedCERTCertList found_certs(CERT_CreateSubjectCertList(
      nullptr /* certList */, CERT_GetDefaultCertDB(), &name,
      PR_Now() /* sorttime */, PR_FALSE /* validOnly */));
  if (!found_certs)
    return;

  for (CERTCertListNode* node = CERT_LIST_HEAD(found_certs);
       !CERT_LIST_END(node, found_certs); node = CERT_LIST_NEXT(node)) {
    CertErrors parse_errors;
    std::shared_ptr<const ParsedCertificate> cur_cert =
        ParsedCertificate::Create(
            x509_util::CreateCryptoBuffer(base::make_span(
                node->cert->derCert.data, node->cert->derCert.len)),
            {}, &parse_errors);

    if (!cur_cert) {
      // TODO(crbug.com/634443): return errors better.
      LOG(ERROR) << "Error parsing issuer certificate:\n"
                 << parse_errors.ToDebugString();
      continue;
    }

    issuers->push_back(std::move(cur_cert));
  }
}

CertificateTrust TrustStoreNSS::GetTrust(const ParsedCertificate* cert,
                                         base::SupportsUserData* debug_data) {
  crypto::EnsureNSSInit();
  if (debug_data) {
    ResultDebugData::Create(ignore_system_trust_settings_,
                            GetSlotFilterType(user_slot_trust_setting_),
                            debug_data);
  }
  // In theory we could also do better multi-profile slot filtering using a
  // similar approach as GetTrustIgnoringSystemTrust, however it makes the
  // logic more complicated and isn't really worth doing since we'll be
  // removing the old path entirely. Also keeping the old path unmodified is
  // better for ensuring that the temporary fallback policy actually falls back
  // to the same old behavior.
  if (ignore_system_trust_settings_) {
    return GetTrustIgnoringSystemTrust(cert, debug_data);
  } else {
    return GetTrustWithSystemTrust(cert, debug_data);
  }
}

std::vector<TrustStoreNSS::ListCertsResult>
TrustStoreNSS::ListCertsIgnoringNSSRoots() {
  std::vector<TrustStoreNSS::ListCertsResult> results;
  crypto::ScopedCERTCertList cert_list;
  if (absl::holds_alternative<crypto::ScopedPK11Slot>(
          user_slot_trust_setting_)) {
    if (absl::get<crypto::ScopedPK11Slot>(user_slot_trust_setting_) ==
        nullptr) {
      return results;
    }
    cert_list.reset(PK11_ListCertsInSlot(
        absl::get<crypto::ScopedPK11Slot>(user_slot_trust_setting_).get()));
  } else {
    cert_list.reset(PK11_ListCerts(PK11CertListUnique, nullptr));
  }
  // PK11_ListCerts[InSlot] can return nullptr, e.g. because the PKCS#11 token
  // that was backing the specified slot is not available anymore.
  // Treat it as no certificates being present on the slot.
  if (!cert_list) {
    LOG(WARNING) << (absl::holds_alternative<crypto::ScopedPK11Slot>(
                         user_slot_trust_setting_)
                         ? "PK11_ListCertsInSlot"
                         : "PK11_ListCerts")
                 << " returned null";
    return results;
  }

  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(cert_list); !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    if (IsCertOnlyInNSSRoots(node->cert)) {
      continue;
    }
    results.emplace_back(x509_util::DupCERTCertificate(node->cert),
                         GetTrustIgnoringSystemTrust(node->cert, nullptr));
  }

  return results;
}

// TODO(https://crbug.com/1340420): add histograms? (how often hits fast vs
// medium vs slow path, timing of fast/medium/slow path/all, etc?)

// TODO(https://crbug.com/1340420): NSS also seemingly has some magical
// trusting of any self-signed cert with CKA_ID=0, if it doesn't have a
// matching trust object. Do we need to do that too? (this pk11_isID0 thing:
// https://searchfox.org/nss/source/lib/pk11wrap/pk11cert.c#357)

CertificateTrust TrustStoreNSS::GetTrustIgnoringSystemTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) const {
  // If trust settings are only being used from a specified slot, and that slot
  // is nullptr, there's nothing to do. This corresponds to the case where we
  // wanted to get the builtin roots from NSS still but not user-added roots.
  // Since the built-in roots are now coming from Chrome Root Store in this
  // case, there is nothing to do here.
  //
  // (This ignores slots that would have been allowed by the "read-only
  // internal slots" part of IsCertAllowedForTrust, I don't think that actually
  // matters though.)
  //
  // TODO(https://crbug.com/1412591): once the non-CRS paths have been removed,
  // perhaps remove this entirely and just have the caller not create a
  // TrustStoreNSS at all in this case (or does it still need the
  // SyncGetIssuersOf to find NSS temp certs in that case?)
  if (absl::holds_alternative<crypto::ScopedPK11Slot>(
          user_slot_trust_setting_) &&
      absl::get<crypto::ScopedPK11Slot>(user_slot_trust_setting_) == nullptr) {
    return CertificateTrust::ForUnspecified();
  }

  SECItem der_cert;
  der_cert.data = const_cast<uint8_t*>(cert->der_cert().UnsafeData());
  der_cert.len = base::checked_cast<unsigned>(cert->der_cert().Length());
  der_cert.type = siDERCertBuffer;

  // Find a matching NSS certificate object, if any. Note that NSS trust
  // objects can also be keyed on issuer+serial and match any such cert. This
  // is only used for distrust and apparently only in the NSS builtin roots
  // certs module. Therefore, it should be safe to use the more efficient
  // CERT_FindCertByDERCert to avoid having to have NSS parse the certificate
  // and create a structure for it if the cert doesn't already exist in any of
  // the loaded NSS databases.
  ScopedCERTCertificate nss_cert(
      CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));
  if (!nss_cert) {
    DVLOG(1) << "skipped cert that has no CERTCertificate already";
    return CertificateTrust::ForUnspecified();
  }

  return GetTrustIgnoringSystemTrust(nss_cert.get(), debug_data);
}

CertificateTrust TrustStoreNSS::GetTrustIgnoringSystemTrust(
    CERTCertificate* nss_cert,
    base::SupportsUserData* debug_data) const {
  // See if NSS has any trust settings for the certificate at all. If not,
  // there is no point in doing further work.
  CERTCertTrust nss_cert_trust;
  if (CERT_GetCertTrust(nss_cert, &nss_cert_trust) != SECSuccess) {
    DVLOG(1) << "skipped cert that has no trust settings";
    return CertificateTrust::ForUnspecified();
  }

  // If there were trust settings, we may not be able to use the NSS calculated
  // trust settings directly, since we don't know which slot those settings
  // came from. Do a more careful check to only honor trust settings from slots
  // we care about.

  std::vector<std::pair<crypto::ScopedPK11Slot, CK_OBJECT_HANDLE>>
      slots_and_handles_for_cert = GetAllSlotsAndHandlesForCert(nss_cert);

  // Generally this shouldn't happen, though it is possible (ex, a builtin
  // distrust record with no matching cert in the builtin trust store could
  // match a NSS temporary cert that doesn't exist in any slot. Ignoring that
  // is okay. Theoretically there maybe could be trust records with no matching
  // cert in user slots? I don't know how that can actually happen though.)
  if (slots_and_handles_for_cert.empty()) {
    DVLOG(1) << "skipped cert that has no slots";
    return CertificateTrust::ForUnspecified();
  }

  // List of trustOrder, slot pairs.
  std::vector<std::pair<int, PK11SlotInfo*>> slots_to_check;

  for (const auto& [slotref, handle] : slots_and_handles_for_cert) {
    PK11SlotInfo* slot = slotref.get();
    DVLOG(1) << "found cert in slot:" << PK11_GetSlotName(slot)
             << " token:" << PK11_GetTokenName(slot)
             << " module trustOrder: " << PK11_GetModule(slot)->trustOrder;
    if (absl::holds_alternative<crypto::ScopedPK11Slot>(
            user_slot_trust_setting_) &&
        slot !=
            absl::get<crypto::ScopedPK11Slot>(user_slot_trust_setting_).get()) {
      DVLOG(1) << "skipping slot " << PK11_GetSlotName(slot)
               << ", it's not user_slot_trust_setting_";
      continue;
    }
    if (IsMozillaCaPolicyProvided(slot, handle)) {
      DVLOG(1) << "skipping slot " << PK11_GetSlotName(slot)
               << ", this is mozilla ca policy provided";
      continue;
    }
    int trust_order = PK11_GetModule(slot)->trustOrder;
    slots_to_check.emplace_back(trust_order, slot);
  }
  if (slots_to_check.size() == slots_and_handles_for_cert.size()) {
    DVLOG(1) << "cert is only in allowed slots, using NSS calculated trust";
    return GetTrustForNSSTrust(nss_cert_trust);
  }
  if (slots_to_check.empty()) {
    DVLOG(1) << "cert is only in disallowed slots, skipping";
    return CertificateTrust::ForUnspecified();
  }

  DVLOG(1) << "cert is in both allowed and disallowed slots, doing manual "
              "trust calculation";

  // Use PK11_FindGenericObjects + PK11_ReadRawAttribute to calculate the trust
  // using only the slots we care about. (Some example code:
  // https://searchfox.org/nss/source/gtests/pk11_gtest/pk11_import_unittest.cc#131)
  //
  // TODO(https://crbug.com/1340420): consider adding caching here if metrics
  // show a need. If caching is added, note that NSS has no change notification
  // APIs so we'd at least want to listen for CertDatabase notifications to
  // clear the cache. (There are multiple approaches possible, could cache the
  // hash->trust mappings on a per-slot basis, or just cache the end result for
  // each cert, etc.)
  base::SHA1Digest cert_sha1 = base::SHA1HashSpan(
      base::make_span(nss_cert->derCert.data, nss_cert->derCert.len));

  // Check the slots in trustOrder ordering. Lower trustOrder values are higher
  // priority, so we can return as soon as we find a matching trust object.
  std::sort(slots_to_check.begin(), slots_to_check.end());

  for (const auto& [_, slot] : slots_to_check) {
    DVLOG(1) << "looking for trust in slot " << PK11_GetSlotName(slot)
             << " token " << PK11_GetTokenName(slot);

    ScopedPK11GenericObjects objs(PK11_FindGenericObjects(slot, CKO_NSS_TRUST));
    if (!objs) {
      DVLOG(1) << "no trust objects in slot";
      continue;
    }
    for (PK11GenericObject* obj = objs.get(); obj != nullptr;
         obj = PK11_GetNextGenericObject(obj)) {
      crypto::ScopedSECItem sha1_hash_attr(SECITEM_AllocItem(/*arena=*/nullptr,
                                                             /*item=*/nullptr,
                                                             /*len=*/0));
      SECStatus rv = PK11_ReadRawAttribute(
          PK11_TypeGeneric, obj, CKA_CERT_SHA1_HASH, sha1_hash_attr.get());
      if (rv != SECSuccess) {
        DVLOG(1) << "trust object has no CKA_CERT_SHA1_HASH attr";
        continue;
      }
      base::span<const uint8_t> trust_obj_sha1 = base::make_span(
          sha1_hash_attr->data, sha1_hash_attr->data + sha1_hash_attr->len);
      DVLOG(1) << "found trust object for sha1 "
               << base::HexEncode(trust_obj_sha1);

      if (!std::equal(trust_obj_sha1.begin(), trust_obj_sha1.end(),
                      cert_sha1.begin(), cert_sha1.end())) {
        DVLOG(1) << "trust object does not match target cert hash, skipping";
        continue;
      }
      DVLOG(1) << "trust object matches target cert hash";

      crypto::ScopedSECItem trust_attr(SECITEM_AllocItem(/*arena=*/nullptr,
                                                         /*item=*/nullptr,
                                                         /*len=*/0));
      rv = PK11_ReadRawAttribute(PK11_TypeGeneric, obj, CKA_TRUST_SERVER_AUTH,
                                 trust_attr.get());
      if (rv != SECSuccess) {
        DVLOG(1) << "trust object for " << base::HexEncode(trust_obj_sha1)
                 << "has no CKA_TRUST_x attr";
        continue;
      }
      DVLOG(1) << "trust "
               << base::HexEncode(base::make_span(
                      trust_attr->data, trust_attr->data + trust_attr->len))
               << " for sha1 " << base::HexEncode(trust_obj_sha1);

      CK_TRUST trust;
      if (trust_attr->len != sizeof(trust)) {
        DVLOG(1) << "trust is wrong size? skipping";
        continue;
      }

      // This matches how pk11_GetTrustField in NSS converts the raw trust
      // object to a CK_TRUST (actually an unsigned long).
      // https://searchfox.org/nss/source/lib/pk11wrap/pk11nobj.c#37
      memcpy(&trust, trust_attr->data, trust_attr->len);

      // This doesn't handle the "TrustAnchorOrLeaf" combination, it's unclear
      // how that is represented. But it doesn't really matter since the only
      // case that would come up is if someone took one of the NSS builtin
      // roots and then also locally marked it as trusted as both a CA and a
      // leaf, which is non-sensical. Testing shows that will end up marked as
      // CKT_NSS_TRUSTED_DELEGATOR, which is fine.
      switch (trust) {
        case CKT_NSS_TRUSTED:
          if (base::FeatureList::IsEnabled(
                  features::kTrustStoreTrustedLeafSupport)) {
            DVLOG(1) << "CKT_NSS_TRUSTED -> trusted leaf";
            return CertificateTrust::ForTrustedLeaf();
          } else {
            DVLOG(1) << "CKT_NSS_TRUSTED -> unspecified";
            return CertificateTrust::ForUnspecified();
          }
        case CKT_NSS_TRUSTED_DELEGATOR: {
          DVLOG(1) << "CKT_NSS_TRUSTED_DELEGATOR -> trust anchor";
          const bool enforce_anchor_constraints =
              IsLocalAnchorConstraintsEnforcementEnabled();
          return CertificateTrust::ForTrustAnchor()
              .WithEnforceAnchorConstraints(enforce_anchor_constraints)
              .WithEnforceAnchorExpiry(enforce_anchor_constraints);
        }
        case CKT_NSS_MUST_VERIFY_TRUST:
        case CKT_NSS_VALID_DELEGATOR:
          DVLOG(1) << "CKT_NSS_MUST_VERIFY_TRUST or CKT_NSS_VALID_DELEGATOR -> "
                      "unspecified";
          return CertificateTrust::ForUnspecified();
        case CKT_NSS_NOT_TRUSTED:
          DVLOG(1) << "CKT_NSS_NOT_TRUSTED -> distrusted";
          return CertificateTrust::ForDistrusted();
        case CKT_NSS_TRUST_UNKNOWN:
          DVLOG(1) << "CKT_NSS_TRUST_UNKNOWN trust value - skip";
          break;
        default:
          DVLOG(1) << "unhandled trust value - skip";
          break;
      }
    }
  }

  DVLOG(1) << "no suitable NSS trust record found";
  return CertificateTrust::ForUnspecified();
}

CertificateTrust TrustStoreNSS::GetTrustWithSystemTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) const {
  // TODO(eroman): Inefficient -- path building will convert between
  // CERTCertificate and ParsedCertificate representations multiple times
  // (when getting the issuers, and again here).

  // Note that trust records in NSS are keyed on issuer + serial, and there
  // exist builtin distrust records for which a matching certificate is not
  // included in the builtin cert list. Therefore, create a temp NSS cert even
  // if no existing cert matches. (Eg, this uses CERT_NewTempCertificate, not
  // CERT_FindCertByDERCert.)
  ScopedCERTCertificate nss_cert(x509_util::CreateCERTCertificateFromBytes(
      cert->der_cert().UnsafeData(), cert->der_cert().Length()));
  if (!nss_cert) {
    return CertificateTrust::ForUnspecified();
  }

  if (!IsCertAllowedForTrust(nss_cert.get())) {
    return CertificateTrust::ForUnspecified();
  }

  // Determine the trustedness of the matched certificate.
  CERTCertTrust nss_trust;
  if (CERT_GetCertTrust(nss_cert.get(), &nss_trust) != SECSuccess) {
    return CertificateTrust::ForUnspecified();
  }

  CertificateTrust trust = GetTrustForNSSTrust(nss_trust);
  if (trust.enforce_anchor_constraints && IsKnownRoot(nss_cert.get())) {
    trust.enforce_anchor_constraints = false;
    trust.enforce_anchor_expiry = false;
  }
  return trust;
}

CertificateTrust TrustStoreNSS::GetTrustForNSSTrust(
    const CERTCertTrust& trust) const {
  unsigned int trust_flags = SEC_GET_TRUST_FLAGS(&trust, trustSSL);

  // Determine if the certificate is distrusted.
  if ((trust_flags & (CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED_CA |
                      CERTDB_TRUSTED)) == CERTDB_TERMINAL_RECORD) {
    return CertificateTrust::ForDistrusted();
  }

  bool is_trusted_ca = false;
  bool is_trusted_leaf = false;
  const bool enforce_anchor_constraints =
      IsLocalAnchorConstraintsEnforcementEnabled();

  // Determine if the certificate is a trust anchor.
  if ((trust_flags & CERTDB_TRUSTED_CA) == CERTDB_TRUSTED_CA) {
    is_trusted_ca = true;
  }

  if (base::FeatureList::IsEnabled(features::kTrustStoreTrustedLeafSupport)) {
    constexpr unsigned int kTrustedPeerBits =
        CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED;
    if ((trust_flags & kTrustedPeerBits) == kTrustedPeerBits) {
      is_trusted_leaf = true;
    }
  }

  if (is_trusted_ca && is_trusted_leaf) {
    return CertificateTrust::ForTrustAnchorOrLeaf()
        .WithEnforceAnchorConstraints(enforce_anchor_constraints)
        .WithEnforceAnchorExpiry(enforce_anchor_constraints);
  } else if (is_trusted_ca) {
    return CertificateTrust::ForTrustAnchor()
        .WithEnforceAnchorConstraints(enforce_anchor_constraints)
        .WithEnforceAnchorExpiry(enforce_anchor_constraints);
  } else if (is_trusted_leaf) {
    return CertificateTrust::ForTrustedLeaf();
  }

  return CertificateTrust::ForUnspecified();
}

bool TrustStoreNSS::IsCertAllowedForTrust(CERTCertificate* cert) const {
  if (absl::holds_alternative<UseTrustFromAllUserSlots>(
          user_slot_trust_setting_)) {
    return true;
  }

  crypto::ScopedPK11SlotList slots_for_cert(
      PK11_GetAllSlotsForCert(cert, nullptr));
  if (!slots_for_cert)
    return false;

  for (PK11SlotListElement* slot_element =
           PK11_GetFirstSafe(slots_for_cert.get());
       slot_element;
       slot_element = PK11_GetNextSafe(slots_for_cert.get(), slot_element,
                                       /*restart=*/PR_FALSE)) {
    PK11SlotInfo* slot = slot_element->slot;
    bool allow_slot =
        // Allow the root certs module.
        PK11_HasRootCerts(slot) ||
        // Allow read-only internal slots.
        (PK11_IsInternal(slot) && !PK11_IsRemovable(slot)) ||
        // Allow configured user slot if specified.
        (absl::holds_alternative<crypto::ScopedPK11Slot>(
             user_slot_trust_setting_) &&
         slot ==
             absl::get<crypto::ScopedPK11Slot>(user_slot_trust_setting_).get());

    if (allow_slot) {
      PK11_FreeSlotListElement(slots_for_cert.get(), slot_element);
      return true;
    }
  }

  return false;
}

}  // namespace net
