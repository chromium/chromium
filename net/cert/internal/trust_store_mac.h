// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_MAC_H_
#define NET_CERT_INTERNAL_TRUST_STORE_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"
#include "net/cert/pki/trust_store.h"

namespace net {

// TrustStoreMac is an implementation of TrustStore which uses macOS keychain
// to find trust anchors for path building. Trust state is cached, so a single
// TrustStoreMac instance should be created and used for all verifications of a
// given policy.
// TrustStoreMac objects are threadsafe and methods may be called from multiple
// threads simultaneously. It is the owner's responsibility to ensure the
// TrustStoreMac object outlives any threads accessing it.
class NET_EXPORT TrustStoreMac : public TrustStore {
 public:
  // Bits representing different conditions encountered while evaluating
  // the trustSettings returned by SecTrustSettingsCopyTrustSettings.
  enum TrustDebugInfo {
    // The trustSettings array was empty.
    TRUST_SETTINGS_ARRAY_EMPTY = 1 << 0,

    // One of the trustSettings dictionaries was empty.
    TRUST_SETTINGS_DICT_EMPTY = 1 << 1,

    // One of the trustSettings dictionaries contained an unknown key.
    TRUST_SETTINGS_DICT_UNKNOWN_KEY = 1 << 2,

    // One of the trustSettings dictionaries contained a
    // kSecTrustSettingsPolicy key.
    TRUST_SETTINGS_DICT_CONTAINS_POLICY = 1 << 3,

    // One of the trustSettings dictionaries contained a
    // kSecTrustSettingsPolicy key with a value that was not a SecPolicyRef.
    TRUST_SETTINGS_DICT_INVALID_POLICY_TYPE = 1 << 4,

    // One of the trustSettings dictionaries contained a
    // kSecTrustSettingsApplication key.
    TRUST_SETTINGS_DICT_CONTAINS_APPLICATION = 1 << 5,

    // One of the trustSettings dictionaries contained a
    // kSecTrustSettingsPolicyString key.
    TRUST_SETTINGS_DICT_CONTAINS_POLICY_STRING = 1 << 6,

    // One of the trustSettings dictionaries contained a
    // kSecTrustSettingsKeyUsage key.
    TRUST_SETTINGS_DICT_CONTAINS_KEY_USAGE = 1 << 7,

    // One of the trustSettings dictionaries contained a
    // kSecTrustSettingsResult key.
    TRUST_SETTINGS_DICT_CONTAINS_RESULT = 1 << 8,

    // One of the trustSettings dictionaries contained a
    // kSecTrustSettingsResult key with a value that was not a CFNumber or
    // could not be represented by a signed int.
    TRUST_SETTINGS_DICT_INVALID_RESULT_TYPE = 1 << 9,

    // One of the trustSettings dictionaries contained a
    // kSecTrustSettingsAllowedError key.
    TRUST_SETTINGS_DICT_CONTAINS_ALLOWED_ERROR = 1 << 10,

    // SecTrustSettingsCopyTrustSettings returned a value other than
    // errSecSuccess or errSecItemNotFound.
    COPY_TRUST_SETTINGS_ERROR = 1 << 11,
  };

  // NOTE: When updating this enum, also update ParamToTrustImplType in
  // system_trust_store.cc
  enum class TrustImplType {
    // Values 1 and 3 were used for implementation strategies that have since
    // been removed.
    kUnknown = 0,
    kSimple = 2,
    kDomainCacheFullCerts = 4,
    kKeychainCacheFullCerts = 5,
  };

  class ResultDebugData : public base::SupportsUserData::Data {
   public:
    static const ResultDebugData* Get(const base::SupportsUserData* debug_data);
    static ResultDebugData* GetOrCreate(base::SupportsUserData* debug_data);

    void UpdateTrustDebugInfo(int trust_debug_info, TrustImplType impl_type);

    // base::SupportsUserData::Data implementation:
    std::unique_ptr<Data> Clone() override;

    // Returns a bitfield of TrustDebugInfo flags. If multiple GetTrust calls
    // were done with the same SupportsUserData object, this will return the
    // union of all the TrustDebugInfo flags.
    int combined_trust_debug_info() const { return combined_trust_debug_info_; }

    // Returns an enum representing which trust implementation was used.
    TrustImplType trust_impl() const { return trust_impl_; }

   private:
    int combined_trust_debug_info_ = 0;

    TrustImplType trust_impl_ = TrustImplType::kUnknown;
  };

  // Creates a TrustStoreMac which will find anchors that are trusted for
  // |policy_oid|. For list of possible policy values, see:
  // https://developer.apple.com/reference/security/1667150-certificate_key_and_trust_servic/1670151-standard_policies_for_specific_c?language=objc
  // |impl| selects which internal implementation is used for checking trust
  // settings.
  TrustStoreMac(CFStringRef policy_oid, TrustImplType impl);

  TrustStoreMac(const TrustStoreMac&) = delete;
  TrustStoreMac& operator=(const TrustStoreMac&) = delete;

  ~TrustStoreMac() override;

  // Initializes the trust cache, if it isn't already initialized.
  void InitializeTrustCache() const;

  // TrustStore implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;
  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override;

 private:
  class TrustImpl;
  class TrustImplDomainCacheFullCerts;
  class TrustImplKeychainCacheFullCerts;
  class TrustImplNoCache;

  // Finds certificates in the OS keychains whose Subject matches |name_data|.
  // The result is an array of CRYPTO_BUFFERs containing the DER certificate
  // data.
  static std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>
  FindMatchingCertificatesForMacNormalizedSubject(CFDataRef name_data);

  // Returns the OS-normalized issuer of |cert|.
  // macOS internally uses a normalized form of subject/issuer names for
  // comparing, roughly similar to RFC3280's normalization scheme. The
  // normalized form is used for any database lookups and comparisons.
  static base::apple::ScopedCFTypeRef<CFDataRef> GetMacNormalizedIssuer(
      const ParsedCertificate* cert);

  std::unique_ptr<TrustImpl> trust_cache_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_MAC_H_
