// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_mac.h"

#include <Security/Security.h>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#include "net/base/hash_value.h"
#include "net/base/network_notification_thread_mac.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parse_name.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/test_keychain_search_list_mac.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_mac.h"
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
    //
    // TODO(mattm): remove the CFCastStrict below once Chromium builds against
    // the 10.11 SDK.
    CFStringRef policy_oid = base::mac::GetValueFromDictionary<CFStringRef>(
        policy_dict, base::mac::CFCastStrict<CFStringRef>(kSecPolicyOid));

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
TrustStatus IsCertificateTrustedForPolicyInDomain(
    const scoped_refptr<ParsedCertificate>& cert,
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
    return TrustStatus::UNSPECIFIED;
  }
  TrustStatus trust = IsTrustSettingsTrustedForPolicy(
      trust_settings, is_self_issued, policy_oid, debug_info);
  return trust;
}

void UpdateUserData(int debug_info, base::SupportsUserData* user_data) {
  if (!user_data)
    return;
  TrustStoreMac::ResultDebugData* result_debug_data =
      TrustStoreMac::ResultDebugData::GetOrCreate(user_data);
  result_debug_data->UpdateTrustDebugInfo(debug_info);
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
  TrustStatus IsCertTrusted(const scoped_refptr<ParsedCertificate>& cert,
                            const SHA256HashValue& cert_hash,
                            base::SupportsUserData* debug_data) {
    auto cache_iter = trust_status_cache_.find(cert_hash);
    if (cache_iter == trust_status_cache_.end()) {
      // Cert does not have trust settings in this domain, return UNSPECIFIED.
      return TrustStatus::UNSPECIFIED;
    }

    if (cache_iter->second.trust_status != TrustStatus::UNKNOWN) {
      // Cert has trust settings and trust has already been calculated, return
      // the cached value.
      UpdateUserData(cache_iter->second.debug_info, debug_data);
      return cache_iter->second.trust_status;
    }

    // Cert has trust settings but trust has not been calculated yet.
    // Calculate it now, insert into cache, and return.
    TrustStatus cert_trust = IsCertificateTrustedForPolicyInDomain(
        cert, policy_oid_, domain_, &cache_iter->second.debug_info);
    cache_iter->second.trust_status = cert_trust;
    UpdateUserData(cache_iter->second.debug_info, debug_data);
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

  DISALLOW_COPY_AND_ASSIGN(TrustDomainCache);
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
  // Registers |callback| to be run when the keychain trust settings change.
  // Must be called on the network notification thread.  |callback| will be run
  // on the network notification thread. The returned Subscription must be
  // destroyed on the network notification thread.
  static std::unique_ptr<base::CallbackList<void()>::Subscription> AddCallback(
      base::RepeatingClosure callback) {
    DCHECK(GetNetworkNotificationThreadMac()->RunsTasksInCurrentSequence());
    return Get()->callback_list_.Add(std::move(callback));
  }

 private:
  friend base::NoDestructor<KeychainTrustSettingsChangedNotifier>;

  KeychainTrustSettingsChangedNotifier() {
    DCHECK(GetNetworkNotificationThreadMac()->RunsTasksInCurrentSequence());
    OSStatus status = SecKeychainAddCallback(
        &KeychainTrustSettingsChangedNotifier::KeychainCallback,
        kSecTrustSettingsChangedEventMask, this);
    if (status != noErr)
      OSSTATUS_LOG(ERROR, status) << "SecKeychainAddCallback failed";
  }

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

  base::CallbackList<void()> callback_list_;

  DISALLOW_COPY_AND_ASSIGN(KeychainTrustSettingsChangedNotifier);
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
  std::unique_ptr<base::CallbackList<void()>::Subscription> subscription_;

  base::subtle::Atomic64 iteration_ = 0;

  DISALLOW_COPY_AND_ASSIGN(KeychainTrustObserver);
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
    int trust_debug_info) {
  combined_trust_debug_info_ |= trust_debug_info;
}

std::unique_ptr<base::SupportsUserData::Data>
TrustStoreMac::ResultDebugData::Clone() {
  return std::make_unique<ResultDebugData>(*this);
}

// TrustCache caches the calculated trust status of certificates with trust
// settings in each of the three trust domains, and ensures the cache is reset
// if trust settings are modified.
class TrustStoreMac::TrustCache {
 public:
  explicit TrustCache(CFStringRef policy_oid)
      : system_domain_cache_(kSecTrustSettingsDomainSystem, policy_oid),
        admin_domain_cache_(kSecTrustSettingsDomainAdmin, policy_oid),
        user_domain_cache_(kSecTrustSettingsDomainUser, policy_oid) {
    keychain_observer_ = std::make_unique<KeychainTrustObserver>();
  }

  ~TrustCache() {
    GetNetworkNotificationThreadMac()->DeleteSoon(
        FROM_HERE, std::move(keychain_observer_));
  }

  // Returns true if |cert| is present in kSecTrustSettingsDomainSystem.
  bool IsKnownRoot(const ParsedCertificate* cert) {
    SHA256HashValue cert_hash = CalculateFingerprint256(cert->der_cert());

    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();
    return system_domain_cache_.ContainsCert(cert_hash);
  }

  // Returns the trust status for |cert|.
  TrustStatus IsCertTrusted(const scoped_refptr<ParsedCertificate>& cert,
                            base::SupportsUserData* debug_data) {
    SHA256HashValue cert_hash = CalculateFingerprint256(cert->der_cert());

    base::AutoLock lock(cache_lock_);
    MaybeInitializeCache();

    // Evaluate trust domains in user, admin, system order. Admin settings can
    // override system ones, and user settings can override both admin and
    // system.
    for (TrustDomainCache* trust_domain_cache :
         {&user_domain_cache_, &admin_domain_cache_, &system_domain_cache_}) {
      TrustStatus ts =
          trust_domain_cache->IsCertTrusted(cert, cert_hash, debug_data);
      if (ts != TrustStatus::UNSPECIFIED)
        return ts;
    }

    // Cert did not have trust settings in any domain.
    return TrustStatus::UNSPECIFIED;
  }

 private:
  // (Re-)Initialize the cache if necessary. Must be called after acquiring
  // |cache_lock_| and before accessing any of the |*_domain_cache_| members.
  void MaybeInitializeCache() {
    cache_lock_.AssertAcquired();
    int64_t keychain_iteration = keychain_observer_->Iteration();
    if (iteration_ == keychain_iteration)
      return;

    iteration_ = keychain_iteration;
    user_domain_cache_.Initialize();
    admin_domain_cache_.Initialize();
    if (!system_domain_initialized_) {
      // In practice, the system trust domain does not change during runtime,
      // and SecTrustSettingsCopyCertificates on the system domain is quite
      // slow, so the system domain cache is not reset on keychain changes.
      system_domain_cache_.Initialize();
      system_domain_initialized_ = true;
    }
  }

  std::unique_ptr<KeychainTrustObserver> keychain_observer_;

  base::Lock cache_lock_;
  // |cache_lock_| must be held while accessing any following members.
  int64_t iteration_ = -1;
  bool system_domain_initialized_ = false;
  TrustDomainCache system_domain_cache_;
  TrustDomainCache admin_domain_cache_;
  TrustDomainCache user_domain_cache_;

  DISALLOW_COPY_AND_ASSIGN(TrustCache);
};

TrustStoreMac::TrustStoreMac(CFTypeRef policy_oid)
    : trust_cache_(std::make_unique<TrustCache>(
          base::mac::CFCastStrict<CFStringRef>(policy_oid))) {}

TrustStoreMac::~TrustStoreMac() = default;

bool TrustStoreMac::IsKnownRoot(const ParsedCertificate* cert) const {
  return trust_cache_->IsKnownRoot(cert);
}

void TrustStoreMac::SyncGetIssuersOf(const ParsedCertificate* cert,
                                     ParsedCertificateList* issuers) {
  base::ScopedCFTypeRef<CFDataRef> name_data = GetMacNormalizedIssuer(cert);
  if (!name_data)
    return;

  base::ScopedCFTypeRef<CFArrayRef> matching_items =
      FindMatchingCertificatesForMacNormalizedSubject(name_data);
  if (!matching_items)
    return;

  // Convert to ParsedCertificate.
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

    CertErrors errors;
    ParseCertificateOptions options;
    options.allow_invalid_serial_numbers = true;
    scoped_refptr<ParsedCertificate> anchor_cert = ParsedCertificate::Create(
        x509_util::CreateCryptoBuffer(CFDataGetBytePtr(der_data.get()),
                                      CFDataGetLength(der_data.get())),
        options, &errors);
    if (!anchor_cert) {
      // TODO(crbug.com/634443): return errors better.
      LOG(ERROR) << "Error parsing issuer certificate:\n"
                 << errors.ToDebugString();
      continue;
    }

    issuers->push_back(std::move(anchor_cert));
  }
}

void TrustStoreMac::GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                             CertificateTrust* trust,
                             base::SupportsUserData* debug_data) const {
  TrustStatus trust_status = trust_cache_->IsCertTrusted(cert, debug_data);
  switch (trust_status) {
    case TrustStatus::TRUSTED:
      *trust = CertificateTrust::ForTrustAnchor();
      return;
    case TrustStatus::DISTRUSTED:
      *trust = CertificateTrust::ForDistrusted();
      return;
    case TrustStatus::UNSPECIFIED:
      *trust = CertificateTrust::ForUnspecified();
      return;
    case TrustStatus::UNKNOWN:
      // UNKNOWN is an implementation detail of TrustCache and should never be
      // returned.
      NOTREACHED();
      break;
  }

  *trust = CertificateTrust::ForUnspecified();
  return;
}

// static
base::ScopedCFTypeRef<CFArrayRef>
TrustStoreMac::FindMatchingCertificatesForMacNormalizedSubject(
    CFDataRef name_data) {
  base::ScopedCFTypeRef<CFArrayRef> matching_items;
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));

  CFDictionarySetValue(query, kSecClass, kSecClassCertificate);
  CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
  CFDictionarySetValue(query, kSecAttrSubject, name_data);

  base::ScopedCFTypeRef<CFArrayRef> scoped_alternate_keychain_search_list;
  if (TestKeychainSearchList::HasInstance()) {
    OSStatus status = TestKeychainSearchList::GetInstance()->CopySearchList(
        scoped_alternate_keychain_search_list.InitializeInto());
    if (status) {
      OSSTATUS_LOG(ERROR, status)
          << "TestKeychainSearchList::CopySearchList error";
      return matching_items;
    }
  }

  base::AutoLock lock(crypto::GetMacSecurityServicesLock());

  // If a TestKeychainSearchList is present, it will have already set
  // |scoped_alternate_keychain_search_list|, which will be used as the
  // basis for reordering the keychain. Otherwise, get the current keychain
  // search list and use that.
  if (!scoped_alternate_keychain_search_list) {
    OSStatus status = SecKeychainCopySearchList(
        scoped_alternate_keychain_search_list.InitializeInto());
    if (status) {
      OSSTATUS_LOG(ERROR, status) << "SecKeychainCopySearchList error";
      return matching_items;
    }
  }

  CFMutableArrayRef mutable_keychain_search_list = CFArrayCreateMutableCopy(
      kCFAllocatorDefault,
      CFArrayGetCount(scoped_alternate_keychain_search_list.get()) + 1,
      scoped_alternate_keychain_search_list.get());
  if (!mutable_keychain_search_list) {
    LOG(ERROR) << "CFArrayCreateMutableCopy";
    return matching_items;
  }
  scoped_alternate_keychain_search_list.reset(mutable_keychain_search_list);

  base::ScopedCFTypeRef<SecKeychainRef> roots_keychain;
  // The System Roots keychain is not normally searched by SecItemCopyMatching.
  // Get a reference to it and include in the keychain search list.
  OSStatus status = SecKeychainOpen(
      "/System/Library/Keychains/SystemRootCertificates.keychain",
      roots_keychain.InitializeInto());
  if (status) {
    OSSTATUS_LOG(ERROR, status) << "SecKeychainOpen error";
    return matching_items;
  }
  CFArrayAppendValue(mutable_keychain_search_list, roots_keychain);

  CFDictionarySetValue(query, kSecMatchSearchList,
                       scoped_alternate_keychain_search_list.get());

  OSStatus err = SecItemCopyMatching(
      query, reinterpret_cast<CFTypeRef*>(matching_items.InitializeInto()));
  if (err == errSecItemNotFound) {
    // No matches found.
    return matching_items;
  }
  if (err) {
    OSSTATUS_LOG(ERROR, err) << "SecItemCopyMatching error";
    return matching_items;
  }
  return matching_items;
}

// static
base::ScopedCFTypeRef<CFDataRef> TrustStoreMac::GetMacNormalizedIssuer(
    const ParsedCertificate* cert) {
  base::ScopedCFTypeRef<CFDataRef> name_data;
  // There does not appear to be any public API to get the normalized version
  // of a Name without creating a SecCertificate.
  base::ScopedCFTypeRef<SecCertificateRef> cert_handle(
      x509_util::CreateSecCertificateFromBytes(cert->der_cert().UnsafeData(),
                                               cert->der_cert().Length()));
  if (!cert_handle) {
    LOG(ERROR) << "CreateCertBufferFromBytes";
    return name_data;
  }
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    name_data.reset(
        SecCertificateCopyNormalizedIssuerContent(cert_handle, nullptr));
  }
  if (!name_data)
    LOG(ERROR) << "SecCertificateCopyNormalizedIssuerContent";
  return name_data;
}

}  // namespace net
