// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_mac.h"

#include <Security/Security.h>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/lru_cache.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#include "net/base/hash_value.h"
#include "net/base/network_notification_thread_mac.h"
#include "net/cert/known_roots_mac.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/cert_issuer_source_static.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/test_keychain_search_list_mac.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_apple.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace net {

namespace {

// The rules for interpreting trust settings are documented at:
// https://developer.apple.com/reference/security/1400261-sectrustsettingscopytrustsetting?language=objc

// Indicates the trust status of a certificate.
enum class TrustStatus {
  // Trust status is unknown / uninitialized.
  UNKNOWN,
  // Certificate inherits trust value from its issuer. If the certificate is the
  // root of the chain, this implies distrust.
  UNSPECIFIED,
  // Certificate is a trust anchor.
  TRUSTED,
  // Certificate is blocked / explicitly distrusted.
  DISTRUSTED
};

enum class KnownRootStatus {
  UNKNOWN,
  IS_KNOWN_ROOT,
  NOT_KNOWN_ROOT,
};

const void* kResultDebugDataKey = &kResultDebugDataKey;

// Returns trust status of usage constraints dictionary |trust_dict| for a
// certificate that |is_self_issued|.
TrustStatus IsTrustDictionaryTrustedForPolicy(
    CFDictionaryRef trust_dict,
    bool is_self_issued,
    const CFStringRef target_policy_oid,
    int* debug_info) {
  // An empty trust dict should be interpreted as
  // kSecTrustSettingsResultTrustRoot. This is handled by falling through all
  // the conditions below with the default value of |trust_settings_result|.
  CFIndex dict_size = CFDictionaryGetCount(trust_dict);
  if (dict_size == 0)
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_EMPTY;

  CFIndex known_elements = 0;
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsPolicy)) {
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_CONTAINS_POLICY;
    known_elements++;
  }
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsApplication)) {
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_CONTAINS_APPLICATION;
    known_elements++;
  }
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsPolicyString)) {
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_CONTAINS_POLICY_STRING;
    known_elements++;
  }
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsKeyUsage)) {
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_CONTAINS_KEY_USAGE;
    known_elements++;
  }
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsResult)) {
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_CONTAINS_RESULT;
    known_elements++;
  }
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsAllowedError)) {
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_CONTAINS_ALLOWED_ERROR;
    known_elements++;
  }
  if (known_elements != dict_size)
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_UNKNOWN_KEY;

  // Trust settings may be scoped to a single application, by checking that the
  // code signing identity of the current application matches the serialized
  // code signing identity in the kSecTrustSettingsApplication key.
  // As this is not presently supported, skip any trust settings scoped to the
  // application.
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsApplication))
    return TrustStatus::UNSPECIFIED;

  // Trust settings may be scoped using policy-specific constraints. For
  // example, SSL trust settings might be scoped to a single hostname, or EAP
  // settings specific to a particular WiFi network.
  // As this is not presently supported, skip any policy-specific trust
  // settings.
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsPolicyString))
    return TrustStatus::UNSPECIFIED;

  // Ignoring kSecTrustSettingsKeyUsage for now; it does not seem relevant to
  // the TLS case.

  // If the trust settings are scoped to a specific policy (via
  // kSecTrustSettingsPolicy), ensure that the policy is the same policy as
  // |target_policy_oid|. If there is no kSecTrustSettingsPolicy key, it's
  // considered a match for all policies.
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsPolicy)) {
    SecPolicyRef policy_ref = base::mac::GetValueFromDictionary<SecPolicyRef>(
        trust_dict, kSecTrustSettingsPolicy);
    if (!policy_ref) {
      *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_INVALID_POLICY_TYPE;
      return TrustStatus::UNSPECIFIED;
    }
    base::ScopedCFTypeRef<CFDictionaryRef> policy_dict;
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      policy_dict.reset(SecPolicyCopyProperties(policy_ref));
    }

    // kSecPolicyOid is guaranteed to be present in the policy dictionary.
    CFStringRef policy_oid = base::mac::GetValueFromDictionary<CFStringRef>(
        policy_dict, kSecPolicyOid);

    if (!CFEqual(policy_oid, target_policy_oid))
      return TrustStatus::UNSPECIFIED;
  }

  // If kSecTrustSettingsResult is not present in the trust dict,
  // kSecTrustSettingsResultTrustRoot is assumed.
  int trust_settings_result = kSecTrustSettingsResultTrustRoot;
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsResult)) {
    CFNumberRef trust_settings_result_ref =
        base::mac::GetValueFromDictionary<CFNumberRef>(trust_dict,
                                                       kSecTrustSettingsResult);
    if (!trust_settings_result_ref ||
        !CFNumberGetValue(trust_settings_result_ref, kCFNumberIntType,
                          &trust_settings_result)) {
      *debug_info |= TrustStoreMac::TRUST_SETTINGS_DICT_INVALID_RESULT_TYPE;
      return TrustStatus::UNSPECIFIED;
    }
  }

  if (trust_settings_result == kSecTrustSettingsResultDeny)
    return TrustStatus::DISTRUSTED;

  // This is a bit of a hack: if the cert is self-issued allow either
  // kSecTrustSettingsResultTrustRoot or kSecTrustSettingsResultTrustAsRoot on
  // the basis that SecTrustSetTrustSettings should not allow creating an
  // invalid trust record in the first place. (The spec is that
  // kSecTrustSettingsResultTrustRoot can only be applied to root(self-signed)
  // certs and kSecTrustSettingsResultTrustAsRoot is used for other certs.)
  // This hack avoids having to check the signature on the cert which is slow
  // if using the platform APIs, and may require supporting MD5 signature
  // algorithms on some older OSX versions or locally added roots, which is
  // undesirable in the built-in signature verifier.
  if (is_self_issued) {
    return (trust_settings_result == kSecTrustSettingsResultTrustRoot ||
            trust_settings_result == kSecTrustSettingsResultTrustAsRoot)
               ? TrustStatus::TRUSTED
               : TrustStatus::UNSPECIFIED;
  }

  // kSecTrustSettingsResultTrustAsRoot can only be applied to non-root certs.
  return (trust_settings_result == kSecTrustSettingsResultTrustAsRoot)
             ? TrustStatus::TRUSTED
             : TrustStatus::UNSPECIFIED;
}

// Returns true if the trust settings array |trust_settings| for a certificate
// that |is_self_issued| should be treated as a trust anchor.
TrustStatus IsTrustSettingsTrustedForPolicy(CFArrayRef trust_settings,
                                            bool is_self_issued,
                                            const CFStringRef policy_oid,
                                            int* debug_info) {
  // An empty trust settings array (that is, the trust_settings parameter
  // returns a valid but empty CFArray) means "always trust this certificate"
  // with an overall trust setting for the certificate of
  // kSecTrustSettingsResultTrustRoot.
  if (CFArrayGetCount(trust_settings) == 0) {
    *debug_info |= TrustStoreMac::TRUST_SETTINGS_ARRAY_EMPTY;
    return is_self_issued ? TrustStatus::TRUSTED : TrustStatus::UNSPECIFIED;
  }

  for (CFIndex i = 0, settings_count = CFArrayGetCount(trust_settings);
       i < settings_count; ++i) {
    CFDictionaryRef trust_dict = reinterpret_cast<CFDictionaryRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(trust_settings, i)));
    TrustStatus trust = IsTrustDictionaryTrustedForPolicy(
        trust_dict, is_self_issued, policy_oid, debug_info);
    if (trust != TrustStatus::UNSPECIFIED)
      return trust;
  }
  return TrustStatus::UNSPECIFIED;
}

// Returns the trust status for |cert_handle| for the policy |policy_oid| in
// |trust_domain|.
TrustStatus IsSecCertificateTrustedForPolicyInDomain(
    SecCertificateRef cert_handle,
    const bool is_self_issued,
    const CFStringRef policy_oid,
    SecTrustSettingsDomain trust_domain,
    int* debug_info) {
  base::ScopedCFTypeRef<CFArrayRef> trust_settings;
  OSStatus err;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    err = SecTrustSettingsCopyTrustSettings(cert_handle, trust_domain,
                                            trust_settings.InitializeInto());
  }
  if (err == errSecItemNotFound) {
    // No trust settings for that domain.. try the next.
    return TrustStatus::UNSPECIFIED;
  }
  if (err) {
    OSSTATUS_LOG(ERROR, err) << "SecTrustSettingsCopyTrustSettings error";
    *debug_info |= TrustStoreMac::COPY_TRUST_SETTINGS_ERROR;
    return TrustStatus::UNSPECIFIED;
  }
  TrustStatus trust = IsTrustSettingsTrustedForPolicy(
      trust_settings, is_self_issued, policy_oid, debug_info);
  return trust;
}

TrustStatus IsCertificateTrustedForPolicyInDomain(
    const ParsedCertificate* cert,
    const CFStringRef policy_oid,
    SecTrustSettingsDomain trust_domain,
    int* debug_info) {
  // TODO(eroman): Inefficient -- path building will convert between
  // SecCertificateRef and ParsedCertificate representations multiple times
  // (when getting the issuers, and again here).
  //
  // This conversion will also be done for each domain the cert policy is
  // checked, but the TrustDomainCache ensures this function is only called on
  // domains that actually have settings for the cert. The common case is that
  // a cert will have trust settings in only zero or one domains, and when in
  // more than one domain it would generally be because one domain is
  // overriding the setting in the next, so it would only get done once anyway.
  base::ScopedCFTypeRef<SecCertificateRef> cert_handle =
      x509_util::CreateSecCertificateFromBytes(cert->der_cert().UnsafeData(),
                                               cert->der_cert().Length());
  if (!cert_handle)
    return TrustStatus::UNSPECIFIED;

  const bool is_self_issued =
      cert->normalized_subject() == cert->normalized_issuer();

  return IsSecCertificateTrustedForPolicyInDomain(
      cert_handle, is_self_issued, policy_oid, trust_domain, debug_info);
}

KnownRootStatus IsCertificateKnownRoot(const ParsedCertificate* cert) {
  base::ScopedCFTypeRef<SecCertificateRef> cert_handle =
      x509_util::CreateSecCertificateFromBytes(cert->der_cert().UnsafeData(),
                                               cert->der_cert().Length());
  if (!cert_handle)
    return KnownRootStatus::NOT_KNOWN_ROOT;

  base::ScopedCFTypeRef<CFArrayRef> trust_settings;
  OSStatus err;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    err = SecTrustSettingsCopyTrustSettings(cert_handle,
                                            kSecTrustSettingsDomainSystem,
                                            trust_settings.InitializeInto());
  }
  return (err == errSecSuccess) ? KnownRootStatus::IS_KNOWN_ROOT
                                : KnownRootStatus::NOT_KNOWN_ROOT;
}

TrustStatus IsCertificateTrustedForPolicy(const ParsedCertificate* cert,
                                          const CFStringRef policy_oid,
                                          TrustStoreMac::TrustDomains domains,
                                          int* debug_info,
                                          KnownRootStatus* out_is_known_root) {
  // |*out_is_known_root| is intentionally not cleared before starting, as
  // there may have been a value already calculated and cached independently.
  // The caller is expected to initialize |*out_is_known_root| to UNKNOWN if
  // the value has not been calculated.

  base::ScopedCFTypeRef<SecCertificateRef> cert_handle =
      x509_util::CreateSecCertificateFromBytes(cert->der_cert().UnsafeData(),
                                               cert->der_cert().Length());
  if (!cert_handle)
    return TrustStatus::UNSPECIFIED;

  const bool is_self_issued =
      cert->normalized_subject() == cert->normalized_issuer();

  // Evaluate trust domains in user, admin, system order. Admin settings can
  // override system ones, and user settings can override both admin and system.
  for (const auto& trust_domain :
       {kSecTrustSettingsDomainUser, kSecTrustSettingsDomainAdmin,
        kSecTrustSettingsDomainSystem}) {
    if (domains == TrustStoreMac::TrustDomains::kUserAndAdmin &&
        trust_domain == kSecTrustSettingsDomainSystem) {
      continue;
    }
    base::ScopedCFTypeRef<CFArrayRef> trust_settings;
    OSStatus err;
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      err = SecTrustSettingsCopyTrustSettings(cert_handle, trust_domain,
                                              trust_settings.InitializeInto());
    }
    if (err != errSecSuccess) {
      if (out_is_known_root && trust_domain == kSecTrustSettingsDomainSystem) {
        // If trust settings are not present for |cert| in the system domain,
        // record it as not a known root.
        *out_is_known_root = KnownRootStatus::NOT_KNOWN_ROOT;
      }
      if (err == errSecItemNotFound) {
        // No trust settings for that domain.. try the next.
        continue;
      }
      OSSTATUS_LOG(ERROR, err) << "SecTrustSettingsCopyTrustSettings error";
      *debug_info |= TrustStoreMac::COPY_TRUST_SETTINGS_ERROR;
      continue;
    }
    if (out_is_known_root && trust_domain == kSecTrustSettingsDomainSystem) {
      // If trust settings are present for |cert| in the system domain, record
      // it as a known root.
      *out_is_known_root = KnownRootStatus::IS_KNOWN_ROOT;
    }
    TrustStatus trust = IsTrustSettingsTrustedForPolicy(
        trust_settings, is_self_issued, policy_oid, debug_info);
    if (trust != TrustStatus::UNSPECIFIED)
      return trust;
  }

  // No trust settings, or none of the settings were for the correct policy, or
  // had the correct trust result.
  return TrustStatus::UNSPECIFIED;
}

void UpdateUserData(int debug_info,
                    base::SupportsUserData* user_data,
                    TrustStoreMac::TrustImplType impl_type) {
  if (!user_data)
    return;
  TrustStoreMac::ResultDebugData* result_debug_data =
      TrustStoreMac::ResultDebugData::GetOrCreate(user_data);
  result_debug_data->UpdateTrustDebugInfo(debug_info, impl_type);
}

// Caches calculated trust status for certificates present in a single trust
// domain.
class TrustDomainCache {
 public:
  struct TrustStatusDetails {
    TrustStatus trust_status = TrustStatus::UNKNOWN;
    int debug_info = 0;
  };

  TrustDomainCache(SecTrustSettingsDomain domain, CFStringRef policy_oid)
      : domain_(domain), policy_oid_(policy_oid) {
    DCHECK(policy_oid_);
  }

  TrustDomainCache(const TrustDomainCache&) = delete;
  TrustDomainCache& operator=(const TrustDomainCache&) = delete;

  // (Re-)Initializes the cache with the certs in |domain_| set to UNKNOWN trust
  // status.
  void Initialize() {
    trust_status_cache_.clear();

    base::ScopedCFTypeRef<CFArrayRef> cert_array;
    OSStatus rv;
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      rv = SecTrustSettingsCopyCertificates(domain_,
                                            cert_array.InitializeInto());
    }
    if (rv != noErr) {
      // Note: SecTrustSettingsCopyCertificates can legitimately return
      // errSecNoTrustSettings if there are no trust settings in |domain_|.
      return;
    }
    std::vector<std::pair<SHA256HashValue, TrustStatusDetails>>
        trust_status_vector;
    for (CFIndex i = 0, size = CFArrayGetCount(cert_array); i < size; ++i) {
      SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(cert_array, i)));
      trust_status_vector.emplace_back(x509_util::CalculateFingerprint256(cert),
                                       TrustStatusDetails());
    }
    trust_status_cache_ = base::flat_map<SHA256HashValue, TrustStatusDetails>(
        std::move(trust_status_vector));
  }

  // Returns the trust status for |cert| in |domain_|.
  TrustStatus IsCertTrusted(const ParsedCertificate* cert,
                            const SHA256HashValue& cert_hash,
                            base::SupportsUserData* debug_data) {
    auto cache_iter = trust_status_cache_.find(cert_hash);
    if (cache_iter == trust_status_cache_.end()) {
      // Cert does not have trust settings in this domain, return UNSPECIFIED.
      UpdateUserData(0, debug_data, TrustStoreMac::TrustImplType::kDomainCache);
      return TrustStatus::UNSPECIFIED;
    }

    if (cache_iter->second.trust_status != TrustStatus::UNKNOWN) {
      // Cert has trust settings and trust has already been calculated, return
      // the cached value.
      UpdateUserData(cache_iter->second.debug_info, debug_data,
                     TrustStoreMac::TrustImplType::kDomainCache);
      return cache_iter->second.trust_status;
    }

    // Cert has trust settings but trust has not been calculated yet.
    // Calculate it now, insert into cache, and return.
    TrustStatus cert_trust = IsCertificateTrustedForPolicyInDomain(
        cert, policy_oid_, domain_, &cache_iter->second.debug_info);
    cache_iter->second.trust_status = cert_trust;
    UpdateUserData(cache_iter->second.debug_info, debug_data,
                   TrustStoreMac::TrustImplType::kDomainCache);
    return cert_trust;
  }

  // Returns true if the certificate with |cert_hash| is present in |domain_|.
  bool ContainsCert(const SHA256HashValue& cert_hash) const {
    return trust_status_cache_.find(cert_hash) != trust_status_cache_.end();
  }

 private:
  const SecTrustSettingsDomain domain_;
  const CFStringRef policy_oid_;
  base::flat_map<SHA256HashValue, TrustStatusDetails> trust_status_cache_;
};

// Caches certificates and calculated trust status for certificates present in
// a single trust domain.
class TrustDomainCacheFullCerts {
 public:
  struct TrustStatusDetails {
    TrustStatus trust_status = TrustStatus::UNKNOWN;
    int debug_info = 0;
  };

  TrustDomainCacheFullCerts(SecTrustSettingsDomain domain,
                            CFStringRef policy_oid)
      : domain_(domain), policy_oid_(policy_oid) {
    DCHECK(policy_oid_);
  }

  TrustDomainCacheFullCerts(const TrustDomainCacheFullCerts&) = delete;
  TrustDomainCacheFullCerts& operator=(const TrustDomainCacheFullCerts&) =
      delete;

  // (Re-)Initializes the cache with the certs in |domain_| set to UNKNOWN trust
  // status.
  void Initialize() {
    trust_status_cache_.clear();
    cert_issuer_source_.Clear();

    base::ScopedCFTypeRef<CFArrayRef> cert_array;
    OSStatus rv;
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      rv = SecTrustSettingsCopyCertificates(domain_,
                                            cert_array.InitializeInto());
    }
    if (rv != noErr) {
      // Note: SecTrustSettingsCopyCertificates can legitimately return
      // errSecNoTrustSettings if there are no trust settings in |domain_|.
      HistogramTrustDomainCertCount(0U);
      return;
    }
    std::vector<std::pair<SHA256HashValue, TrustStatusDetails>>
        trust_status_vector;
    for (CFIndex i = 0, size = CFArrayGetCount(cert_array); i < size; ++i) {
      SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(cert_array, i)));
      base::ScopedCFTypeRef<CFDataRef> der_data(SecCertificateCopyData(cert));
      if (!der_data) {
        LOG(ERROR) << "SecCertificateCopyData error";
        continue;
      }
      auto buffer = x509_util::CreateCryptoBuffer(base::make_span(
          CFDataGetBytePtr(der_data.get()), CFDataGetLength(der_data.get())));
      CertErrors errors;
      ParseCertificateOptions options;
      options.allow_invalid_serial_numbers = true;
      scoped_refptr<ParsedCertificate> parsed_cert =
          ParsedCertificate::Create(std::move(buffer), options, &errors);
      if (!parsed_cert) {
        LOG(ERROR) << "Error parsing certificate:\n" << errors.ToDebugString();
        continue;
      }
      cert_issuer_source_.AddCert(parsed_cert);
      trust_status_vector.emplace_back(x509_util::CalculateFingerprint256(cert),
                                       TrustStatusDetails());
    }
    HistogramTrustDomainCertCount(trust_status_vector.size());
    trust_status_cache_ = base::flat_map<SHA256HashValue, TrustStatusDetails>(
        std::move(trust_status_vector));
  }

  // Returns the trust status for |cert| in |domain_|.
  TrustStatus IsCertTrusted(const ParsedCertificate* cert,
                            const SHA256HashValue& cert_hash,
                            base::SupportsUserData* debug_data) {
    auto cache_iter = trust_status_cache_.find(cert_hash);
    if (cache_iter == trust_status_cache_.end()) {
      // Cert does not have trust settings in this domain, return UNSPECIFIED.
      UpdateUserData(0, debug_data,
                     TrustStoreMac::TrustImplType::kDomainCacheFullCerts);
      return TrustStatus::UNSPECIFIED;
    }

    if (cache_iter->second.trust_status != TrustStatus::UNKNOWN) {
      // Cert has trust settings and trust has already been calculated, return
      // the cached value.
      UpdateUserData(cache_iter->second.debug_info, debug_data,
                     TrustStoreMac::TrustImplType::kDomainCacheFullCerts);
      return cache_iter->second.trust_status;
    }

    // Cert has trust settings but trust has not been calculated yet.
    // Calculate it now, insert into cache, and return.
    TrustStatus cert_trust = IsCertificateTrustedForPolicyInDomain(
        cert, policy_oid_, domain_, &cache_iter->second.debug_info);
    cache_iter->second.trust_status = cert_trust;
    UpdateUserData(cache_iter->second.debug_info, debug_data,
                   TrustStoreMac::TrustImplType::kDomainCacheFullCerts);
    return cert_trust;
  }

  // Returns true if the certificate with |cert_hash| is present in |domain_|.
  bool ContainsCert(const SHA256HashValue& cert_hash) const {
    return trust_status_cache_.find(cert_hash) != trust_status_cache_.end();
  }

  // Returns a CertIssuerSource containing all the certificates that are
  // present in |domain_|.
  CertIssuerSource& cert_issuer_source() { return cert_issuer_source_; }

 private:
  void HistogramTrustDomainCertCount(size_t count) const {
    base::StringPiece domain_name;
    switch (domain_) {
      case kSecTrustSettingsDomainUser:
        domain_name = "User";
        break;
      case kSecTrustSettingsDomainAdmin:
        domain_name = "Admin";
        break;
      case kSecTrustSettingsDomainSystem:
        domain_name = "System";
        break;
    }
    base::UmaHistogramCounts1000(
        base::StrCat(
            {"Net.CertVerifier.MacTrustDomainCertCount.", domain_name}),
        count);
  }

  const SecTrustSettingsDomain domain_;
  const CFStringRef policy_oid_;
  base::flat_map<SHA256HashValue, TrustStatusDetails> trust_status_cache_;
  CertIssuerSourceStatic cert_issuer_source_;
};

SHA256HashValue CalculateFingerprint256(const der::Input& buffer) {
  SHA256HashValue sha256;
  SHA256(buffer.UnsafeData(), buffer.Length(), sha256.data);
  return sha256;
}

// Watches macOS keychain for trust setting changes, and notifies any
// registered callbacks. This is necessary as the keychain callback API is
// keyed only on the callback function pointer rather than function pointer +
// context, so it cannot be safely registered multiple callbacks with the same
// function pointer and different contexts.
class KeychainTrustSettingsChangedNotifier {
 public:
  KeychainTrustSettingsChangedNotifier(
      const KeychainTrustSettingsChangedNotifier&) = delete;
  KeychainTrustSettingsChangedNotifier& operator=(
      const KeychainTrustSettingsChangedNotifier&) = delete;

  // Registers |callback| to be run when the keychain trust settings change.
  // Must be called on the network notification thread.  |callback| will be run
  // on the network notification thread. The returned subscription must be
  // destroyed on the network notification thread.
  static base::CallbackListSubscription AddCallback(
      base::RepeatingClosure callback) {
    DCHECK(GetNetworkNotificationThreadMac()->RunsTasksInCurrentSequence());
    return Get()->callback_list_.Add(std::move(callback));
  }

 private:
  friend base::NoDestructor<KeychainTrustSettingsChangedNotifier>;

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251 but deprecation
// warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

  KeychainTrustSettingsChangedNotifier() {
    DCHECK(GetNetworkNotificationThreadMac()->RunsTasksInCurrentSequence());
    OSStatus status = SecKeychainAddCallback(
        &KeychainTrustSettingsChangedNotifier::KeychainCallback,
        kSecTrustSettingsChangedEventMask, this);
    if (status != noErr)
      OSSTATUS_LOG(ERROR, status) << "SecKeychainAddCallback failed";
  }

#pragma clang diagnostic pop

  ~KeychainTrustSettingsChangedNotifier() = delete;

  static OSStatus KeychainCallback(SecKeychainEvent keychain_event,
                                   SecKeychainCallbackInfo* info,
                                   void* context) {
    KeychainTrustSettingsChangedNotifier* notifier =
        reinterpret_cast<KeychainTrustSettingsChangedNotifier*>(context);
    notifier->callback_list_.Notify();
    return errSecSuccess;
  }

  static KeychainTrustSettingsChangedNotifier* Get() {
    static base::NoDestructor<KeychainTrustSettingsChangedNotifier> notifier;
    return notifier.get();
  }

  base::RepeatingClosureList callback_list_;
};

// Observes keychain events and increments the value returned by Iteration()
// each time the trust settings change.
class KeychainTrustObserver {
 public:
  KeychainTrustObserver() {
    GetNetworkNotificationThreadMac()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &KeychainTrustObserver::RegisterCallbackOnNotificationThread,
            base::Unretained(this)));
  }

  KeychainTrustObserver(const KeychainTrustObserver&) = delete;
  KeychainTrustObserver& operator=(const KeychainTrustObserver&) = delete;

  // Destroying the observer unregisters the callback. Must be destroyed on the
  // notification thread in order to safely release |subscription_|.
  ~KeychainTrustObserver() {
    DCHECK(GetNetworkNotificationThreadMac()->RunsTasksInCurrentSequence());
  }

  // Returns the current iteration count, which is incremented every time
  // keychain trust settings change. This may be called from any thread.
  int64_t Iteration() const { return base::subtle::Acquire_Load(&iteration_); }

 private:
  void RegisterCallbackOnNotificationThread() {
    DCHECK(GetNetworkNotificationThreadMac()->RunsTasksInCurrentSequence());
    subscription_ =
        KeychainTrustSettingsChangedNotifier::AddCallback(base::BindRepeating(
            &KeychainTrustObserver::Increment, base::Unretained(this)));
  }

  void Increment() { base::subtle::Barrier_AtomicIncrement(&iteration_, 1); }

  // Only accessed on the notification thread.
  base::CallbackListSubscription subscription_;

  base::subtle::Atomic64 iteration_ = 0;
};

}  // namespace

// static
const TrustStoreMac::ResultDebugData* TrustStoreMac::ResultDebugData::Get(
    const base::SupportsUserData* debug_data) {
  return static_cast<ResultDebugData*>(
      debug_data->GetUserData(kResultDebugDataKey));
}

// static
TrustStoreMac::ResultDebugData* TrustStoreMac::ResultDebugData::GetOrCreate(
    base::SupportsUserData* debug_data) {
  ResultDebugData* data = static_cast<ResultDebugData*>(
      debug_data->GetUserData(kResultDebugDataKey));
  if (!data) {
    std::unique_ptr<ResultDebugData> new_data =
        std::make_unique<ResultDebugData>();
    data = new_data.get();
    debug_data->SetUserData(kResultDebugDataKey, std::move(new_data));
  }
  return data;
}

void TrustStoreMac::ResultDebugData::UpdateTrustDebugInfo(
    int trust_debug_info,
    TrustImplType impl_type) {
  combined_trust_debug_info_ |= trust_debug_info;
  trust_impl_ = impl_type;
}

std::unique_ptr<base::SupportsUserData::Data>
TrustStoreMac::ResultDebugData::Clone() {
  return std::make_unique<ResultDebugData>(*this);
}

// Interface for different implementations of getting trust settings from the
// Mac APIs. This abstraction can be removed once a single implementation has
// been chosen and launched.
class TrustStoreMac::TrustImpl {
 public:
  virtual ~TrustImpl() = default;

  virtual bool IsKnownRoot(const ParsedCertificate* cert) = 0;
  virtual TrustStatus IsCertTrusted(const ParsedCertificate* cert,
                                    base::SupportsUserData* debug_data) = 0;
  virtual bool ImplementsSyncGetIssuersOf() const { return false; }
  virtual void SyncGetIssuersOf(const ParsedCertificate* cert,
                                ParsedCertificateList* issuers) {}
  virtual void InitializeTrustCache() = 0;
};

// TrustImplDomainCache uses SecTrustSettingsCopyCertificates to get the list
// of certs in each trust domain and then caches the calculated trust status of
// those certs on access, and ensures the cache is reset if trust settings are
// modified.
class TrustStoreMac::TrustImplDomainCache : public TrustStoreMac::TrustImpl {
 public:
  explicit TrustImplDomainCache(CFStringRef policy_oid, TrustDomains domains)
      : use_system_domain_cache_(domains == TrustDomains::kAll),
        admin_domain_cache_(kSecTrustSettingsDomainAdmin, policy_oid),
        user_domain_cache_(kSecTrustSettingsDomainUser, policy_oid) {
    if (use_system_domain_cache_) {
      system_domain_cache_ = std::make_unique<TrustDomainCache>(
          kSecTrustSettingsDomainSystem, policy_oid);
    }
    keychain_observer_ = std::make_unique<KeychainTrustObserver>();
  }

  TrustImplDomainCache(const TrustImplDomainCache&) = delete;
  TrustImplDomainCache& operator=(const TrustImplDomainCache&) = delete;

  ~TrustImplDomainCache() override {
    GetNetworkNotificationThreadMac()->DeleteSoon(
        FROM_HERE, std::move(keychain_observer_));
  }

  // Returns true if |cert| is present in kSecTrustSettingsDomainSystem.
  bool IsKnownRoot(const ParsedCertificate* cert) override {
    if (!use_system_domain_cache_)
      return false;
    SHA256HashValue cert_hash = CalculateFingerprint256(cert->der_cert());

    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();
    return system_domain_cache_->ContainsCert(cert_hash);
  }

  // Returns the trust status for |cert|.
  TrustStatus IsCertTrusted(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override {
    SHA256HashValue cert_hash = CalculateFingerprint256(cert->der_cert());

    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();

    // Evaluate trust domains in user, admin, system order. Admin settings can
    // override system ones, and user settings can override both admin and
    // system.
    for (TrustDomainCache* trust_domain_cache :
         {&user_domain_cache_, &admin_domain_cache_}) {
      TrustStatus ts =
          trust_domain_cache->IsCertTrusted(cert, cert_hash, debug_data);
      if (ts != TrustStatus::UNSPECIFIED)
        return ts;
    }
    if (use_system_domain_cache_) {
      return system_domain_cache_->IsCertTrusted(cert, cert_hash, debug_data);
    }

    // Cert did not have trust settings in any domain.
    return TrustStatus::UNSPECIFIED;
  }

  // Initializes the cache, if it isn't already initialized.
  void InitializeTrustCache() override {
    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();
  }

 private:
  // (Re-)Initialize the cache if necessary. Must be called after acquiring
  // |cache_lock_| and before accessing any of the |*_domain_cache_| members.
  void MaybeInitializeCache() EXCLUSIVE_LOCKS_REQUIRED(cache_lock_) {
    cache_lock_.AssertAcquired();
    int64_t keychain_iteration = keychain_observer_->Iteration();
    if (iteration_ == keychain_iteration)
      return;

    iteration_ = keychain_iteration;
    user_domain_cache_.Initialize();
    admin_domain_cache_.Initialize();
    if (use_system_domain_cache_ && !system_domain_initialized_) {
      // In practice, the system trust domain does not change during runtime,
      // and SecTrustSettingsCopyCertificates on the system domain is quite
      // slow, so the system domain cache is not reset on keychain changes.
      system_domain_cache_->Initialize();
      system_domain_initialized_ = true;
    }
  }

  std::unique_ptr<KeychainTrustObserver> keychain_observer_;
  // Store whether to use the system domain in a const bool that is initialized
  // in constructor so it is safe to read without having to lock first.
  const bool use_system_domain_cache_;

  base::Lock cache_lock_;
  // |cache_lock_| must be held while accessing any following members.
  int64_t iteration_ GUARDED_BY(cache_lock_) = -1;
  bool system_domain_initialized_ GUARDED_BY(cache_lock_) = false;
  std::unique_ptr<TrustDomainCache> system_domain_cache_
      GUARDED_BY(cache_lock_);
  TrustDomainCache admin_domain_cache_ GUARDED_BY(cache_lock_);
  TrustDomainCache user_domain_cache_ GUARDED_BY(cache_lock_);
};

// TrustImplDomainCacheFullCerts uses SecTrustSettingsCopyCertificates to get
// the list of certs in each trust domain and caches the full certificates so
// that pathbuilding does not need to touch any Mac APIs unless one of those
// certificates is encountered, at which point the calculated trust status of
// that cert is cached. The cache is reset if trust settings are modified.
class TrustStoreMac::TrustImplDomainCacheFullCerts
    : public TrustStoreMac::TrustImpl {
 public:
  explicit TrustImplDomainCacheFullCerts(CFStringRef policy_oid,
                                         TrustDomains domains)
      : use_system_domain_cache_(domains == TrustDomains::kAll),
        admin_domain_cache_(kSecTrustSettingsDomainAdmin, policy_oid),
        user_domain_cache_(kSecTrustSettingsDomainUser, policy_oid) {
    if (use_system_domain_cache_) {
      system_domain_cache_ = std::make_unique<TrustDomainCacheFullCerts>(
          kSecTrustSettingsDomainSystem, policy_oid);
    }
    keychain_observer_ = std::make_unique<KeychainTrustObserver>();
  }

  TrustImplDomainCacheFullCerts(const TrustImplDomainCacheFullCerts&) = delete;
  TrustImplDomainCacheFullCerts& operator=(
      const TrustImplDomainCacheFullCerts&) = delete;

  ~TrustImplDomainCacheFullCerts() override {
    GetNetworkNotificationThreadMac()->DeleteSoon(
        FROM_HERE, std::move(keychain_observer_));
  }

  // Returns true if |cert| is present in kSecTrustSettingsDomainSystem.
  bool IsKnownRoot(const ParsedCertificate* cert) override {
    if (!use_system_domain_cache_)
      return false;
    SHA256HashValue cert_hash = CalculateFingerprint256(cert->der_cert());

    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();
    return system_domain_cache_->ContainsCert(cert_hash);
  }

  // Returns the trust status for |cert|.
  TrustStatus IsCertTrusted(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override {
    SHA256HashValue cert_hash = CalculateFingerprint256(cert->der_cert());

    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();

    // Evaluate trust domains in user, admin, system order. Admin settings can
    // override system ones, and user settings can override both admin and
    // system.
    for (TrustDomainCacheFullCerts* trust_domain_cache :
         {&user_domain_cache_, &admin_domain_cache_}) {
      TrustStatus ts =
          trust_domain_cache->IsCertTrusted(cert, cert_hash, debug_data);
      if (ts != TrustStatus::UNSPECIFIED)
        return ts;
    }
    if (use_system_domain_cache_) {
      return system_domain_cache_->IsCertTrusted(cert, cert_hash, debug_data);
    }

    // Cert did not have trust settings in any domain.
    return TrustStatus::UNSPECIFIED;
  }

  bool ImplementsSyncGetIssuersOf() const override { return true; }

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override {
    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();
    user_domain_cache_.cert_issuer_source().SyncGetIssuersOf(cert, issuers);
    admin_domain_cache_.cert_issuer_source().SyncGetIssuersOf(cert, issuers);
    if (system_domain_cache_) {
      system_domain_cache_->cert_issuer_source().SyncGetIssuersOf(cert,
                                                                  issuers);
    }
  }

  // Initializes the cache, if it isn't already initialized.
  void InitializeTrustCache() override {
    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();
  }

 private:
  // (Re-)Initialize the cache if necessary. Must be called after acquiring
  // |cache_lock_| and before accessing any of the |*_domain_cache_| members.
  void MaybeInitializeCache() EXCLUSIVE_LOCKS_REQUIRED(cache_lock_) {
    cache_lock_.AssertAcquired();
    int64_t keychain_iteration = keychain_observer_->Iteration();
    if (iteration_ == keychain_iteration)
      return;

    iteration_ = keychain_iteration;
    user_domain_cache_.Initialize();
    admin_domain_cache_.Initialize();
    if (use_system_domain_cache_ && !system_domain_initialized_) {
      // In practice, the system trust domain does not change during runtime,
      // and SecTrustSettingsCopyCertificates on the system domain is quite
      // slow, so the system domain cache is not reset on keychain changes.
      system_domain_cache_->Initialize();
      system_domain_initialized_ = true;
    }
  }

  std::unique_ptr<KeychainTrustObserver> keychain_observer_;
  // Store whether to use the system domain in a const bool that is initialized
  // in constructor so it is safe to read without having to lock first.
  const bool use_system_domain_cache_;

  base::Lock cache_lock_;
  // |cache_lock_| must be held while accessing any following members.
  int64_t iteration_ GUARDED_BY(cache_lock_) = -1;
  bool system_domain_initialized_ GUARDED_BY(cache_lock_) = false;
  std::unique_ptr<TrustDomainCacheFullCerts> system_domain_cache_
      GUARDED_BY(cache_lock_);
  TrustDomainCacheFullCerts admin_domain_cache_ GUARDED_BY(cache_lock_);
  TrustDomainCacheFullCerts user_domain_cache_ GUARDED_BY(cache_lock_);
};

// TrustImplNoCache is the simplest approach which calls
// SecTrustSettingsCopyTrustSettings on every cert checked, with no caching.
class TrustStoreMac::TrustImplNoCache : public TrustStoreMac::TrustImpl {
 public:
  explicit TrustImplNoCache(CFStringRef policy_oid, TrustDomains domains)
      : policy_oid_(policy_oid), domains_(domains) {}

  TrustImplNoCache(const TrustImplNoCache&) = delete;
  TrustImplNoCache& operator=(const TrustImplNoCache&) = delete;

  ~TrustImplNoCache() override = default;

  // Returns true if |cert| is present in kSecTrustSettingsDomainSystem.
  bool IsKnownRoot(const ParsedCertificate* cert) override {
    if (domains_ == TrustDomains::kUserAndAdmin)
      return false;
    HashValue cert_hash(CalculateFingerprint256(cert->der_cert()));
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    return net::IsKnownRoot(cert_hash);
  }

  // Returns the trust status for |cert|.
  TrustStatus IsCertTrusted(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override {
    int debug_info = 0;
    TrustStatus result =
        IsCertificateTrustedForPolicy(cert, policy_oid_, domains_, &debug_info,
                                      /*out_is_known_root=*/nullptr);
    UpdateUserData(debug_info, debug_data,
                   TrustStoreMac::TrustImplType::kSimple);
    return result;
  }

  void InitializeTrustCache() override {
    // No-op for this impl.
  }

 private:
  const CFStringRef policy_oid_;
  const TrustDomains domains_;
};

// TrustImplLRUCache is calls SecTrustSettingsCopyTrustSettings on every cert
// checked, but caches the results in an LRU cache. The cache is cleared on
// keychain updates.
class TrustStoreMac::TrustImplLRUCache : public TrustStoreMac::TrustImpl {
 public:
  TrustImplLRUCache(CFStringRef policy_oid,
                    size_t cache_size,
                    TrustDomains domains)
      : policy_oid_(policy_oid),
        domains_(domains),
        trust_status_cache_(cache_size) {
    keychain_observer_ = std::make_unique<KeychainTrustObserver>();
  }

  TrustImplLRUCache(const TrustImplLRUCache&) = delete;
  TrustImplLRUCache& operator=(const TrustImplLRUCache&) = delete;

  ~TrustImplLRUCache() override {
    GetNetworkNotificationThreadMac()->DeleteSoon(
        FROM_HERE, std::move(keychain_observer_));
  }

  // Returns true if |cert| has trust settings in kSecTrustSettingsDomainSystem.
  bool IsKnownRoot(const ParsedCertificate* cert) override {
    if (domains_ == TrustDomains::kUserAndAdmin)
      return false;
    return GetKnownRootStatus(cert) == KnownRootStatus::IS_KNOWN_ROOT;
  }

  // Returns the trust status for |cert|.
  TrustStatus IsCertTrusted(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override {
    TrustStatusDetails trust_details = GetTrustStatus(cert);
    UpdateUserData(trust_details.debug_info, debug_data,
                   TrustStoreMac::TrustImplType::kLruCache);
    return trust_details.trust_status;
  }

  void InitializeTrustCache() override {
    // No-op for this impl.
  }

 private:
  struct TrustStatusDetails {
    TrustStatus trust_status = TrustStatus::UNKNOWN;
    int debug_info = 0;
    KnownRootStatus is_known_root = KnownRootStatus::UNKNOWN;
  };

  KnownRootStatus GetKnownRootStatus(const ParsedCertificate* cert) {
    SHA256HashValue cert_hash = CalculateFingerprint256(cert->der_cert());

    int starting_cache_iteration = -1;
    {
      base::AutoLock lock(cache_lock_);
      MaybeResetCache();
      starting_cache_iteration = iteration_;
      auto cache_iter = trust_status_cache_.Get(cert_hash);
      if (cache_iter != trust_status_cache_.end() &&
          cache_iter->second.is_known_root != KnownRootStatus::UNKNOWN) {
        return cache_iter->second.is_known_root;
      }
    }

    KnownRootStatus is_known_root = IsCertificateKnownRoot(cert);

    {
      base::AutoLock lock(cache_lock_);
      MaybeResetCache();
      if (iteration_ != starting_cache_iteration)
        return is_known_root;

      auto cache_iter = trust_status_cache_.Get(cert_hash);
      // Update |is_known_root| on existing cache entry if there is one,
      // otherwise create a new cache entry.
      if (cache_iter != trust_status_cache_.end()) {
        cache_iter->second.is_known_root = is_known_root;
      } else {
        TrustStatusDetails trust_details;
        trust_details.is_known_root = is_known_root;
        trust_status_cache_.Put(cert_hash, trust_details);
      }
    }
    return is_known_root;
  }

  TrustStatusDetails GetTrustStatus(const ParsedCertificate* cert) {
    SHA256HashValue cert_hash = CalculateFingerprint256(cert->der_cert());
    TrustStatusDetails trust_details;

    int starting_cache_iteration = -1;
    {
      base::AutoLock lock(cache_lock_);
      MaybeResetCache();
      starting_cache_iteration = iteration_;
      auto cache_iter = trust_status_cache_.Get(cert_hash);
      if (cache_iter != trust_status_cache_.end()) {
        if (cache_iter->second.trust_status != TrustStatus::UNKNOWN)
          return cache_iter->second;
        // If there was a cache entry but the trust status was not initialized,
        // copy the existing values. (|is_known_root| might already be cached.)
        trust_details = cache_iter->second;
      }
    }

    trust_details.trust_status = IsCertificateTrustedForPolicy(
        cert, policy_oid_, domains_, &trust_details.debug_info,
        &trust_details.is_known_root);

    {
      base::AutoLock lock(cache_lock_);
      MaybeResetCache();
      if (iteration_ != starting_cache_iteration)
        return trust_details;
      trust_status_cache_.Put(cert_hash, trust_details);
    }
    return trust_details;
  }

  void MaybeResetCache() EXCLUSIVE_LOCKS_REQUIRED(cache_lock_) {
    cache_lock_.AssertAcquired();
    int64_t keychain_iteration = keychain_observer_->Iteration();
    if (iteration_ == keychain_iteration)
      return;
    iteration_ = keychain_iteration;
    trust_status_cache_.Clear();
  }

  const CFStringRef policy_oid_;
  const TrustDomains domains_;
  std::unique_ptr<KeychainTrustObserver> keychain_observer_;

  base::Lock cache_lock_;
  // |cache_lock_| must be held while accessing any following members.
  base::LRUCache<SHA256HashValue, TrustStatusDetails> trust_status_cache_
      GUARDED_BY(cache_lock_);
  // Tracks the number of keychain changes that have been observed. If the
  // keychain observer has noted a change, MaybeResetCache will update
  // |iteration_| and the cache will be cleared. Any in-flight trust
  // resolutions that started before the keychain update was observed should
  // not cache their results, as it isn't clear whether the calculated result
  // applies to the new or old trust settings.
  int64_t iteration_ GUARDED_BY(cache_lock_) = -1;
};

TrustStoreMac::TrustStoreMac(CFStringRef policy_oid,
                             TrustImplType impl,
                             size_t cache_size,
                             TrustDomains domains)
    : domains_(domains) {
  switch (impl) {
    case TrustImplType::kUnknown:
      DCHECK(false);
      break;
    case TrustImplType::kDomainCache:
      trust_cache_ =
          std::make_unique<TrustImplDomainCache>(policy_oid, domains);
      break;
    case TrustImplType::kSimple:
      trust_cache_ = std::make_unique<TrustImplNoCache>(policy_oid, domains);
      break;
    case TrustImplType::kLruCache:
      trust_cache_ =
          std::make_unique<TrustImplLRUCache>(policy_oid, cache_size, domains);
      break;
    case TrustImplType::kDomainCacheFullCerts:
      trust_cache_ =
          std::make_unique<TrustImplDomainCacheFullCerts>(policy_oid, domains);
      break;
  }
}

TrustStoreMac::~TrustStoreMac() = default;

void TrustStoreMac::InitializeTrustCache() const {
  trust_cache_->InitializeTrustCache();
}

bool TrustStoreMac::IsKnownRoot(const ParsedCertificate* cert) const {
  return trust_cache_->IsKnownRoot(cert);
}

void TrustStoreMac::SyncGetIssuersOf(const ParsedCertificate* cert,
                                     ParsedCertificateList* issuers) {
  if (trust_cache_->ImplementsSyncGetIssuersOf()) {
    trust_cache_->SyncGetIssuersOf(cert, issuers);
    return;
  }

  base::ScopedCFTypeRef<CFDataRef> name_data = GetMacNormalizedIssuer(cert);
  if (!name_data)
    return;

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> matching_cert_buffers =
      FindMatchingCertificatesForMacNormalizedSubject(name_data, domains_);

  // Convert to ParsedCertificate.
  for (auto& buffer : matching_cert_buffers) {
    CertErrors errors;
    ParseCertificateOptions options;
    options.allow_invalid_serial_numbers = true;
    scoped_refptr<ParsedCertificate> anchor_cert =
        ParsedCertificate::Create(std::move(buffer), options, &errors);
    if (!anchor_cert) {
      // TODO(crbug.com/634443): return errors better.
      LOG(ERROR) << "Error parsing issuer certificate:\n"
                 << errors.ToDebugString();
      continue;
    }

    issuers->push_back(std::move(anchor_cert));
  }
}

CertificateTrust TrustStoreMac::GetTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) const {
  TrustStatus trust_status = trust_cache_->IsCertTrusted(cert, debug_data);
  switch (trust_status) {
    case TrustStatus::TRUSTED:
      return CertificateTrust::ForTrustAnchorEnforcingExpiration();
    case TrustStatus::DISTRUSTED:
      return CertificateTrust::ForDistrusted();
    case TrustStatus::UNSPECIFIED:
      return CertificateTrust::ForUnspecified();
    case TrustStatus::UNKNOWN:
      // UNKNOWN is an implementation detail of TrustImpl and should never be
      // returned.
      NOTREACHED();
      break;
  }

  return CertificateTrust::ForUnspecified();
}

// static
std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>
TrustStoreMac::FindMatchingCertificatesForMacNormalizedSubject(
    CFDataRef name_data,
    TrustDomains domains) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> matching_cert_buffers;
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));

  CFDictionarySetValue(query, kSecClass, kSecClassCertificate);
  CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
  CFDictionarySetValue(query, kSecAttrSubject, name_data);

  base::AutoLock lock(crypto::GetMacSecurityServicesLock());

  base::ScopedCFTypeRef<CFArrayRef> scoped_alternate_keychain_search_list;
  if (TestKeychainSearchList::HasInstance()) {
    OSStatus status = TestKeychainSearchList::GetInstance()->CopySearchList(
        scoped_alternate_keychain_search_list.InitializeInto());
    if (status) {
      OSSTATUS_LOG(ERROR, status)
          << "TestKeychainSearchList::CopySearchList error";
      return matching_cert_buffers;
    }
  }

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251 but deprecation
// warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

  if (domains == TrustDomains::kAll) {
    // If a TestKeychainSearchList is present, it will have already set
    // |scoped_alternate_keychain_search_list|, which will be used as the
    // basis for reordering the keychain. Otherwise, get the current keychain
    // search list and use that.
    if (!scoped_alternate_keychain_search_list) {
      OSStatus status = SecKeychainCopySearchList(
          scoped_alternate_keychain_search_list.InitializeInto());
      if (status) {
        OSSTATUS_LOG(ERROR, status) << "SecKeychainCopySearchList error";
        return matching_cert_buffers;
      }
    }

    CFMutableArrayRef mutable_keychain_search_list = CFArrayCreateMutableCopy(
        kCFAllocatorDefault,
        CFArrayGetCount(scoped_alternate_keychain_search_list.get()) + 1,
        scoped_alternate_keychain_search_list.get());
    if (!mutable_keychain_search_list) {
      LOG(ERROR) << "CFArrayCreateMutableCopy";
      return matching_cert_buffers;
    }
    scoped_alternate_keychain_search_list.reset(mutable_keychain_search_list);

    base::ScopedCFTypeRef<SecKeychainRef> roots_keychain;
    // The System Roots keychain is not normally searched by
    // SecItemCopyMatching. Get a reference to it and include in the keychain
    // search list.
    OSStatus status = SecKeychainOpen(
        "/System/Library/Keychains/SystemRootCertificates.keychain",
        roots_keychain.InitializeInto());
    if (status) {
      OSSTATUS_LOG(ERROR, status) << "SecKeychainOpen error";
      return matching_cert_buffers;
    }
    CFArrayAppendValue(mutable_keychain_search_list, roots_keychain);
  }

#pragma clang diagnostic pop

  if (scoped_alternate_keychain_search_list) {
    CFDictionarySetValue(query, kSecMatchSearchList,
                         scoped_alternate_keychain_search_list.get());
  }

  base::ScopedCFTypeRef<CFArrayRef> matching_items;
  OSStatus err = SecItemCopyMatching(
      query, reinterpret_cast<CFTypeRef*>(matching_items.InitializeInto()));
  if (err == errSecItemNotFound) {
    // No matches found.
    return matching_cert_buffers;
  }
  if (err) {
    OSSTATUS_LOG(ERROR, err) << "SecItemCopyMatching error";
    return matching_cert_buffers;
  }

  for (CFIndex i = 0, item_count = CFArrayGetCount(matching_items);
       i < item_count; ++i) {
    SecCertificateRef match_cert_handle = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(matching_items, i)));

    base::ScopedCFTypeRef<CFDataRef> der_data(
        SecCertificateCopyData(match_cert_handle));
    if (!der_data) {
      LOG(ERROR) << "SecCertificateCopyData error";
      continue;
    }
    matching_cert_buffers.push_back(x509_util::CreateCryptoBuffer(
        base::make_span(CFDataGetBytePtr(der_data.get()),
                        CFDataGetLength(der_data.get()))));
  }
  return matching_cert_buffers;
}

// static
base::ScopedCFTypeRef<CFDataRef> TrustStoreMac::GetMacNormalizedIssuer(
    const ParsedCertificate* cert) {
  base::ScopedCFTypeRef<CFDataRef> name_data;
  base::AutoLock lock(crypto::GetMacSecurityServicesLock());
  // There does not appear to be any public API to get the normalized version
  // of a Name without creating a SecCertificate.
  base::ScopedCFTypeRef<SecCertificateRef> cert_handle(
      x509_util::CreateSecCertificateFromBytes(cert->der_cert().UnsafeData(),
                                               cert->der_cert().Length()));
  if (!cert_handle) {
    LOG(ERROR) << "CreateCertBufferFromBytes";
    return name_data;
  }
  name_data.reset(SecCertificateCopyNormalizedIssuerSequence(cert_handle));
  if (!name_data)
    LOG(ERROR) << "SecCertificateCopyNormalizedIssuerContent";
  return name_data;
}

}  // namespace net
