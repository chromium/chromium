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

  // Creates a TrustStoreNSS which will find anchors that are trusted for
  // |trust_type|.
  //
  // |system_trust_setting| configures the use of trust from the builtin roots.
  // If |system_trust_setting| is kIgnoreSystemTrust, trust settings from the
  // builtin roots slot with the Mozilla CA Policy attribute will not be used.
  //
  // |user_slot_trust_setting| configures the use of trust from user slots:
  //  * UseTrustFromAllUserSlots: all user slots will be allowed.
  //  * nullptr: no user slots will be allowed.
  //  * non-null PK11Slot: the specified slot will be allowed.
  TrustStoreNSS(SECTrustType trust_type,
                SystemTrustSetting system_trust_setting,
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

 private:
  bool IsCertAllowedForTrust(CERTCertificate* cert) const;

  SECTrustType trust_type_;

  // |ignore_system_certs_trust_settings_| specifies if the system trust
  // settings should be considered when determining a cert's trustworthiness.
  //
  // TODO(hchao, sleevi): Figure out how to ignore built-in trust settings,
  // while respecting user-configured trust settings, for these certificates.
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
