// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_MAC_H_
#define NET_CERT_INTERNAL_TRUST_STORE_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/cert/internal/trust_store.h"

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
  };

  class ResultDebugData : public base::SupportsUserData::Data {
   public:
    static const ResultDebugData* Get(const base::SupportsUserData* debug_data);
    static ResultDebugData* GetOrCreate(base::SupportsUserData* debug_data);

    void UpdateTrustDebugInfo(int trust_debug_info);

    // base::SupportsUserData::Data implementation:
    std::unique_ptr<Data> Clone() override;

    // Returns a bitfield of TrustDebugInfo flags. If multiple GetTrust calls
    // were done with the same SupportsUserData object, this will return the
    // union of all the TrustDebugInfo flags.
    int combined_trust_debug_info() const { return combined_trust_debug_info_; }

   private:
    int combined_trust_debug_info_ = 0;
  };

  // Creates a TrustStoreMac which will find anchors that are trusted for
  // |policy_oid|. For list of possible policy values, see:
  // https://developer.apple.com/reference/security/1667150-certificate_key_and_trust_servic/1670151-standard_policies_for_specific_c?language=objc
  // TODO(mattm): policy oids are actually CFStrings, but the constants are
  // defined as CFTypeRef in older SDK versions. Change |policy_oid| type to
  // const CFStringRef when Chromium switches to building against the 10.11 SDK
  // (or newer).
  explicit TrustStoreMac(CFTypeRef policy_oid);
  ~TrustStoreMac() override;

  // Returns true if the given certificate is present in the system trust
  // domain.
  bool IsKnownRoot(const ParsedCertificate* cert) const;

  // TrustStore implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;
  void GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                CertificateTrust* trust,
                base::SupportsUserData* debug_data) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TrustStoreMacTest, MultiRootNotTrusted);
  FRIEND_TEST_ALL_PREFIXES(TrustStoreMacTest, SystemCerts);

  class TrustCache;

  // Finds certificates in the OS keychains whose Subject matches |name_data|.
  // The result is an array of SecCertificateRef.
  static base::ScopedCFTypeRef<CFArrayRef>
  FindMatchingCertificatesForMacNormalizedSubject(CFDataRef name_data);

  // Returns the OS-normalized issuer of |cert|.
  // macOS internally uses a normalized form of subject/issuer names for
  // comparing, roughly similar to RFC3280's normalization scheme. The
  // normalized form is used for any database lookups and comparisons.
  static base::ScopedCFTypeRef<CFDataRef> GetMacNormalizedIssuer(
      const ParsedCertificate* cert);

  std::unique_ptr<TrustCache> trust_cache_;

  DISALLOW_COPY_AND_ASSIGN(TrustStoreMac);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_MAC_H_
