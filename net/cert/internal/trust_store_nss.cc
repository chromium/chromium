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

#include "base/containers/to_vector.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "crypto/chaps_support.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/features.h"
#include "net/cert/internal/platform_trust_store.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store.h"

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_DEVICE)
// TODO(crbug.com/40281745): We can remove these weak attributes in M123 or
// later. Until then, these need to be declared with the weak attribute
// since older platforms may not provide these symbols.
extern "C" CERTCertList* CERT_CreateSubjectCertListForChromium(
    CERTCertList* certList,
    CERTCertDBHandle* handle,
    const SECItem* name,
    PRTime sorttime,
    PRBool validOnly,
    PRBool ignoreChaps) __attribute__((weak));
extern "C" CERTCertificate* CERT_FindCertByDERCertForChromium(
    CERTCertDBHandle* handle,
    SECItem* derCert,
    PRBool ignoreChaps) __attribute__((weak));
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_DEVICE)

namespace net {

namespace {

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
GetAllSlotsAndHandlesForCert(CERTCertificate* nss_cert,
                             bool ignore_chaps_module) {
  std::vector<std::pair<crypto::ScopedPK11Slot, CK_OBJECT_HANDLE>> r;
  crypto::AutoSECMODListReadLock lock_id;
  for (const SECMODModuleList* item = SECMOD_GetDefaultModuleList();
       item != nullptr; item = item->next) {
#if BUILDFLAG(IS_CHROMEOS)
    if (ignore_chaps_module && crypto::IsChapsModule(item->module)) {
      // This check avoids unnecessary IPCs between NSS and Chaps.
      continue;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)

    // SAFETY: item->module->slots is an array with item->module->slotCount
    // elements. slotCount is a signed int so use checked_cast when creating
    // the span.
    base::span<PK11SlotInfo*> module_slots = UNSAFE_BUFFERS(
        base::span(item->module->slots,
                   base::checked_cast<size_t>(item->module->slotCount)));

    for (PK11SlotInfo* slot : module_slots) {
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
  // In this path, `cert` could be a client certificate, so we should not skip
  // the chaps module.
  std::vector<std::pair<crypto::ScopedPK11Slot, CK_OBJECT_HANDLE>>
      slots_and_handles_for_cert =
          GetAllSlotsAndHandlesForCert(cert, /*ignore_chaps_module=*/false);
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

TrustStoreNSS::ListCertsResult::ListCertsResult(ScopedCERTCertificate cert,
                                                bssl::CertificateTrust trust)
    : cert(std::move(cert)), trust(trust) {}
TrustStoreNSS::ListCertsResult::~ListCertsResult() = default;

TrustStoreNSS::ListCertsResult::ListCertsResult(ListCertsResult&& other) =
    default;
TrustStoreNSS::ListCertsResult& TrustStoreNSS::ListCertsResult::operator=(
    ListCertsResult&& other) = default;

TrustStoreNSS::TrustStoreNSS(UserSlotTrustSetting user_slot_trust_setting)
    : user_slot_trust_setting_(std::move(user_slot_trust_setting)) {
  if (absl::holds_alternative<crypto::ScopedPK11Slot>(
          user_slot_trust_setting_)) {
    CHECK(absl::get<crypto::ScopedPK11Slot>(user_slot_trust_setting_) !=
          nullptr);
  }
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_DEVICE)
  if (!CERT_CreateSubjectCertListForChromium) {
    LOG(WARNING) << "CERT_CreateSubjectCertListForChromium is not available";
  }
  if (!CERT_FindCertByDERCertForChromium) {
    LOG(WARNING) << "CERT_FindCertByDERCertForChromium is not available";
  }
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_DEVICE)
}

TrustStoreNSS::~TrustStoreNSS() = default;

void TrustStoreNSS::SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                                     bssl::ParsedCertificateList* issuers) {
  crypto::EnsureNSSInit();

  SECItem name;
  // Use the original issuer value instead of the normalized version. NSS does a
  // less extensive normalization in its Name comparisons, so our normalized
  // version may not match the unnormalized version.
  name.len = cert->tbs().issuer_tlv.size();
  name.data = const_cast<uint8_t*>(cert->tbs().issuer_tlv.data());

  // |validOnly| in CERT_CreateSubjectCertList controls whether to return only
  // certs that are valid at |sorttime|. Expiration isn't meaningful for trust
  // anchors, so request all the matches.
#if !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(IS_CHROMEOS_DEVICE)
  crypto::ScopedCERTCertList found_certs(CERT_CreateSubjectCertList(
      nullptr /* certList */, CERT_GetDefaultCertDB(), &name,
      PR_Now() /* sorttime */, PR_FALSE /* validOnly */));
#else
  crypto::ScopedCERTCertList found_certs;
  if (CERT_CreateSubjectCertListForChromium) {
    found_certs =
        crypto::ScopedCERTCertList(CERT_CreateSubjectCertListForChromium(
            nullptr /* certList */, CERT_GetDefaultCertDB(), &name,
            PR_Now() /* sorttime */, PR_FALSE /* validOnly */,
            PR_TRUE /* ignoreChaps */));
  } else {
    found_certs = crypto::ScopedCERTCertList(CERT_CreateSubjectCertList(
        nullptr /* certList */, CERT_GetDefaultCertDB(), &name,
        PR_Now() /* sorttime */, PR_FALSE /* validOnly */));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(IS_CHROMEOS_DEVICE)

  if (!found_certs) {
    return;
  }

  for (CERTCertListNode* node = CERT_LIST_HEAD(found_certs);
       !CERT_LIST_END(node, found_certs); node = CERT_LIST_NEXT(node)) {
    bssl::CertErrors parse_errors;
    std::shared_ptr<const bssl::ParsedCertificate> cur_cert =
        bssl::ParsedCertificate::Create(
            x509_util::CreateCryptoBuffer(
                x509_util::CERTCertificateAsSpan(node->cert)),
            {}, &parse_errors);

    if (!cur_cert) {
      // TODO(crbug.com/41267838): return errors better.
      LOG(ERROR) << "Error parsing issuer certificate:\n"
                 << parse_errors.ToDebugString();
      continue;
    }

    issuers->push_back(std::move(cur_cert));
  }
}

std::vector<TrustStoreNSS::ListCertsResult>
TrustStoreNSS::ListCertsIgnoringNSSRoots() {
  crypto::EnsureNSSInit();
  std::vector<TrustStoreNSS::ListCertsResult> results;
  crypto::ScopedCERTCertList cert_list;
  if (absl::holds_alternative<crypto::ScopedPK11Slot>(
          user_slot_trust_setting_)) {
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
                         GetTrustIgnoringSystemTrust(node->cert));
  }

  return results;
}

// TODO(crbug.com/40850344): add histograms? (how often hits fast vs
// medium vs slow path, timing of fast/medium/slow path/all, etc?)

// TODO(crbug.com/40850344): NSS also seemingly has some magical
// trusting of any self-signed cert with CKA_ID=0, if it doesn't have a
// matching trust object. Do we need to do that too? (this pk11_isID0 thing:
// https://searchfox.org/nss/source/lib/pk11wrap/pk11cert.c#357)

bssl::CertificateTrust TrustStoreNSS::GetTrust(
    const bssl::ParsedCertificate* cert) {
  crypto::EnsureNSSInit();

  SECItem der_cert;
  der_cert.data = const_cast<uint8_t*>(cert->der_cert().data());
  der_cert.len = base::checked_cast<unsigned>(cert->der_cert().size());
  der_cert.type = siDERCertBuffer;

  // Find a matching NSS certificate object, if any. Note that NSS trust
  // objects can also be keyed on issuer+serial and match any such cert. This
  // is only used for distrust and apparently only in the NSS builtin roots
  // certs module. Therefore, it should be safe to use the more efficient
  // CERT_FindCertByDERCert to avoid having to have NSS parse the certificate
  // and create a structure for it if the cert doesn't already exist in any of
  // the loaded NSS databases.
#if !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(IS_CHROMEOS_DEVICE)
  ScopedCERTCertificate nss_cert(
      CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));
#else
  ScopedCERTCertificate nss_cert;
  if (CERT_FindCertByDERCertForChromium) {
    nss_cert = ScopedCERTCertificate(CERT_FindCertByDERCertForChromium(
        CERT_GetDefaultCertDB(), &der_cert, /*ignoreChaps=*/PR_TRUE));
  } else {
    nss_cert = ScopedCERTCertificate(
        CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(IS_CHROMEOS_DEVICE)

  if (!nss_cert) {
    DVLOG(1) << "skipped cert that has no CERTCertificate already";
    return bssl::CertificateTrust::ForUnspecified();
  }

  return GetTrustIgnoringSystemTrust(nss_cert.get());
}

bssl::CertificateTrust TrustStoreNSS::GetTrustIgnoringSystemTrust(
    CERTCertificate* nss_cert) const {
  // See if NSS has any trust settings for the certificate at all. If not,
  // there is no point in doing further work.
  CERTCertTrust nss_cert_trust;
  if (CERT_GetCertTrust(nss_cert, &nss_cert_trust) != SECSuccess) {
    DVLOG(1) << "skipped cert that has no trust settings";
    return bssl::CertificateTrust::ForUnspecified();
  }

  // If there were trust settings, we may not be able to use the NSS calculated
  // trust settings directly, since we don't know which slot those settings
  // came from. Do a more careful check to only honor trust settings from slots
  // we care about.

  // We expect that CERT_GetCertTrust() != SECSuccess for client certs stored in
  // Chaps. So, `nss_cert` should be a CA certificate and should not be stored
  // in Chaps. Thus, we don't scan the chaps module in the following call for
  // performance reasons.
  std::vector<std::pair<crypto::ScopedPK11Slot, CK_OBJECT_HANDLE>>
      slots_and_handles_for_cert =
          GetAllSlotsAndHandlesForCert(nss_cert, /*ignore_chaps_module=*/true);

  // Generally this shouldn't happen, though it is possible (ex, a builtin
  // distrust record with no matching cert in the builtin trust store could
  // match a NSS temporary cert that doesn't exist in any slot. Ignoring that
  // is okay. Theoretically there maybe could be trust records with no matching
  // cert in user slots? I don't know how that can actually happen though.)
  if (slots_and_handles_for_cert.empty()) {
    DVLOG(1) << "skipped cert that has no slots";
    return bssl::CertificateTrust::ForUnspecified();
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
    return bssl::CertificateTrust::ForUnspecified();
  }

  DVLOG(1) << "cert is in both allowed and disallowed slots, doing manual "
              "trust calculation";

  // Use PK11_FindGenericObjects + PK11_ReadRawAttribute to calculate the trust
  // using only the slots we care about. (Some example code:
  // https://searchfox.org/nss/source/gtests/pk11_gtest/pk11_import_unittest.cc#131)
  //
  // TODO(crbug.com/40850344): consider adding caching here if metrics
  // show a need. If caching is added, note that NSS has no change notification
  // APIs so we'd at least want to listen for CertDatabase notifications to
  // clear the cache. (There are multiple approaches possible, could cache the
  // hash->trust mappings on a per-slot basis, or just cache the end result for
  // each cert, etc.)
  base::SHA1Digest cert_sha1 =
      base::SHA1Hash(x509_util::CERTCertificateAsSpan(nss_cert));

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
      base::span<const uint8_t> trust_obj_sha1 =
          x509_util::SECItemAsSpan(*sha1_hash_attr);
      DVLOG(1) << "found trust object for sha1 "
               << base::HexEncode(trust_obj_sha1);

      if (trust_obj_sha1 != cert_sha1) {
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
               << base::HexEncode(x509_util::SECItemAsSpan(*trust_attr))
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
          DVLOG(1) << "CKT_NSS_TRUSTED -> trusted leaf";
          return bssl::CertificateTrust::ForTrustedLeaf();
        case CKT_NSS_TRUSTED_DELEGATOR: {
          DVLOG(1) << "CKT_NSS_TRUSTED_DELEGATOR -> trust anchor";
          return bssl::CertificateTrust::ForTrustAnchor()
              .WithEnforceAnchorConstraints()
              .WithEnforceAnchorExpiry();
        }
        case CKT_NSS_MUST_VERIFY_TRUST:
        case CKT_NSS_VALID_DELEGATOR:
          DVLOG(1) << "CKT_NSS_MUST_VERIFY_TRUST or CKT_NSS_VALID_DELEGATOR -> "
                      "unspecified";
          return bssl::CertificateTrust::ForUnspecified();
        case CKT_NSS_NOT_TRUSTED:
          DVLOG(1) << "CKT_NSS_NOT_TRUSTED -> distrusted";
          return bssl::CertificateTrust::ForDistrusted();
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
  return bssl::CertificateTrust::ForUnspecified();
}

bssl::CertificateTrust TrustStoreNSS::GetTrustForNSSTrust(
    const CERTCertTrust& trust) const {
  unsigned int trust_flags = SEC_GET_TRUST_FLAGS(&trust, trustSSL);

  // Determine if the certificate is distrusted.
  if ((trust_flags & (CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED_CA |
                      CERTDB_TRUSTED)) == CERTDB_TERMINAL_RECORD) {
    return bssl::CertificateTrust::ForDistrusted();
  }

  // Determine if the certificate is a trust anchor.
  bool is_trusted_ca = (trust_flags & CERTDB_TRUSTED_CA) == CERTDB_TRUSTED_CA;

  constexpr unsigned int kTrustedPeerBits =
      CERTDB_TERMINAL_RECORD | CERTDB_TRUSTED;
  bool is_trusted_leaf = (trust_flags & kTrustedPeerBits) == kTrustedPeerBits;

  if (is_trusted_ca && is_trusted_leaf) {
    return bssl::CertificateTrust::ForTrustAnchorOrLeaf()
        .WithEnforceAnchorConstraints()
        .WithEnforceAnchorExpiry();
  } else if (is_trusted_ca) {
    return bssl::CertificateTrust::ForTrustAnchor()
        .WithEnforceAnchorConstraints()
        .WithEnforceAnchorExpiry();
  } else if (is_trusted_leaf) {
    return bssl::CertificateTrust::ForTrustedLeaf();
  }

  return bssl::CertificateTrust::ForUnspecified();
}

std::vector<PlatformTrustStore::CertWithTrust>
TrustStoreNSS::GetAllUserAddedCerts() {
  std::vector<PlatformTrustStore::CertWithTrust> user_added_certs;
  for (const auto& cert_result : ListCertsIgnoringNSSRoots()) {
    // Skip user certs, unless the user added the user cert with specific
    // server auth trust settings.
    if (cert_result.trust.HasUnspecifiedTrust() &&
        CERT_IsUserCert(cert_result.cert.get())) {
      continue;
    }

    user_added_certs.emplace_back(
        base::ToVector(
            x509_util::CERTCertificateAsSpan(cert_result.cert.get())),
        cert_result.trust);
  }

  return user_added_certs;
}

}  // namespace net
