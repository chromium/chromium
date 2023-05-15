// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_NSS_H_
#define NET_CERT_INTERNAL_TRUST_STORE_NSS_H_

#include <cert.h>
#include <certt.h>

#include "crypto/scoped_nss_types.h"
#include "net/base/net_export.h"
#include "net/cert/pki/trust_store.h"
#include "net/cert/scoped_nss_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace net {

// TrustStoreNSS is an implementation of TrustStore which uses NSS to find trust
// anchors for path building. This TrustStore is thread-safe.
class NET_EXPORT TrustStoreNSS : public TrustStore {
 public:
  enum SystemTrustSetting {
    kUseSystemTrust,
    kIgnoreSystemTrust,
  };

  struct UseTrustFromAllUserSlots : absl::monostate {};
  using UserSlotTrustSetting =
      absl::variant<UseTrustFromAllUserSlots, crypto::ScopedPK11Slot>;

  class ResultDebugData : public base::SupportsUserData::Data {
   public:
    enum class SlotFilterType {
      kDontFilter,
      kDoNotAllowUserSlots,
      kAllowSpecifiedUserSlot
    };

    explicit ResultDebugData(bool ignore_system_trust_settings,
                             SlotFilterType slot_filter_type);

    static const ResultDebugData* Get(const base::SupportsUserData* debug_data);
    static void Create(bool ignore_system_trust_settings,
                       SlotFilterType slot_filter_type,
                       base::SupportsUserData* debug_data);

    // base::SupportsUserData::Data implementation:
    std::unique_ptr<Data> Clone() override;

    bool ignore_system_trust_settings() const {
      return ignore_system_trust_settings_;
    }

    SlotFilterType slot_filter_type() const { return slot_filter_type_; }

   private:
    const bool ignore_system_trust_settings_;
    const SlotFilterType slot_filter_type_;
  };

  // Creates a TrustStoreNSS which will find anchors that are trusted for
  // SSL server auth.
  //
  // |system_trust_setting| configures the use of trust from the builtin roots.
  // If |system_trust_setting| is kIgnoreSystemTrust, trust settings from the
  // builtin roots slot with the Mozilla CA Policy attribute will not be used.
  //
  // |user_slot_trust_setting| configures the use of trust from user slots:
  //  * UseTrustFromAllUserSlots: all user slots will be allowed.
  //  * nullptr: no user slots will be allowed.
  //  * non-null PK11Slot: the specified slot will be allowed.
  TrustStoreNSS(SystemTrustSetting system_trust_setting,
                UserSlotTrustSetting user_slot_trust_setting);

  TrustStoreNSS(const TrustStoreNSS&) = delete;
  TrustStoreNSS& operator=(const TrustStoreNSS&) = delete;

  ~TrustStoreNSS() override;

  // CertIssuerSource implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;

  // TrustStore implementation:
  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override;

  struct ListCertsResult {
    ListCertsResult(ScopedCERTCertificate cert, CertificateTrust trust);
    ~ListCertsResult();
    ListCertsResult(ListCertsResult&& other);
    ListCertsResult& operator=(ListCertsResult&& other);

    ScopedCERTCertificate cert;
    CertificateTrust trust;
  };
  std::vector<ListCertsResult> ListCertsIgnoringNSSRoots();

 private:
  bool IsCertAllowedForTrust(CERTCertificate* cert) const;
  CertificateTrust GetTrustForNSSTrust(const CERTCertTrust& trust) const;

  CertificateTrust GetTrustIgnoringSystemTrust(
      const ParsedCertificate* cert,
      base::SupportsUserData* debug_data) const;

  CertificateTrust GetTrustIgnoringSystemTrust(
      CERTCertificate* nss_cert,
      base::SupportsUserData* debug_data) const;

  CertificateTrust GetTrustWithSystemTrust(
      const ParsedCertificate* cert,
      base::SupportsUserData* debug_data) const;

  // |ignore_system_certs_trust_settings_| specifies if the system trust
  // settings should be considered when determining a cert's trustworthiness.
  const bool ignore_system_trust_settings_ = false;

  // |user_slot_trust_setting_| specifies which slots certificates must be
  // stored on to be allowed to be trusted. The possible values are:
  //
  // |user_slot_trust_setting_| is UseTrustFromAllUserSlots: Allow trust
  // settings from any user slots.
  //
  // |user_slot_trust_setting_| is a ScopedPK11Slot: Allow
  // certificates from the specified slot to be trusted. If the slot is nullptr,
  // trust from user slots will not be used.
  const UserSlotTrustSetting user_slot_trust_setting_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_NSS_H_
