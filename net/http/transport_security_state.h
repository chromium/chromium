// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TRANSPORT_SECURITY_STATE_H_
#define NET_HTTP_TRANSPORT_SECURITY_STATE_H_

#include <stdint.h>

#include <array>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/hash.h"
#include "net/base/expiring_cache.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/cert/key_pinning_state.h"
#include "net/cert/require_ct_delegate.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/transport_security_state_source.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(CRONET_BUILD)
#include "net/cert/key_pinning_state_dynamic.h"
#elif BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
#include "net/cert/key_pinning_state_chrome.h"
#endif

namespace net {

namespace ct {
enum class CTPolicyCompliance;
}

class X509Certificate;

void NET_EXPORT_PRIVATE SetTransportSecurityStateSourceForTesting(
    const TransportSecurityStateSource* source);

// Whether an insecure connection should be upgraded to use SSL. For metrics
// this includes whether the decision came from static or dynamic state.
enum class SSLUpgradeDecision {
  // No state indicated an upgrade.
  kNoUpgrade,
  // Dynamic state indicated an upgrade.
  kDynamicUpgrade,
  // Static state indicated an upgrade. If dynamic state existed, it gave the
  // same result as the static state.
  kStaticUpgrade,
};

// Tracks which hosts have enabled strict transport security.
//
// This object manages the in-memory store. Register a Delegate with
// |SetDelegate| to persist the state to disk.
//
// HTTP strict transport security (HSTS) is defined in
// http://tools.ietf.org/html/ietf-websec-strict-transport-sec.
class NET_EXPORT TransportSecurityState {
 public:
  using HashedHost = std::array<uint8_t, crypto::hash::kSha256Size>;

  // TODO(crbug.com/497882860): For simplicity during the refactor, alias types
  // that used to be subclasses of TransportSecurityState. Remove these.
  using PKPState = KeyPinningState::PKPState;
  using PKPStatus = KeyPinningState::PKPStatus;
#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
  using PinSet = KeyPinningStateChrome::PinSet;
  using PinSetInfo = KeyPinningStateChrome::PinSetInfo;
#endif

  class NET_EXPORT Delegate {
   public:
    // This function may not block and may be called with internal locks held.
    // Thus it must not reenter the TransportSecurityState object.
    virtual void StateIsDirty(TransportSecurityState* state) = 0;
    // Same as StateIsDirty but instructs the Delegate to persist the data
    // immediately and call |callback| when done.
    virtual void WriteNow(TransportSecurityState* state,
                          base::OnceClosure callback) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // A STSState describes the strict transport security state (required
  // upgrade to HTTPS).
  class NET_EXPORT STSState {
   public:
    enum UpgradeMode {
      // These numbers must match those in hsts_view.js, function modeToString.
      MODE_FORCE_HTTPS = 0,
      MODE_DEFAULT = 1,
    };

    STSState();
    ~STSState();

    // The absolute time (UTC) when the |upgrade_mode| (and other state) was
    // observed.
    base::Time last_observed;

    // The absolute time (UTC) when |upgrade_mode| (and other state)
    // expires.
    base::Time expiry;

    UpgradeMode upgrade_mode = MODE_DEFAULT;

    // Are subdomains subject to this policy state?
    bool include_subdomains = false;

    // The domain which matched during a search for this STSState entry.
    // Updated by |GetDynamicSTSState| and |GetStaticDomainState|.
    std::string domain;

    // ShouldUpgradeToSSL returns true iff HTTP requests should be internally
    // redirected to HTTPS (also if WS should be upgraded to WSS).
    bool ShouldUpgradeToSSL() const;
  };

  class NET_EXPORT STSStateIterator {
   public:
    explicit STSStateIterator(const TransportSecurityState& state);
    ~STSStateIterator();

    bool HasNext() const { return iterator_ != end_; }
    void Advance() { ++iterator_; }
    const HashedHost& hostname() const { return iterator_->first; }
    const STSState& domain_state() const { return iterator_->second; }

   private:
    std::map<HashedHost, STSState>::const_iterator iterator_;
    std::map<HashedHost, STSState>::const_iterator end_;
  };

  TransportSecurityState();

  // Creates a TransportSecurityState object that will skip the check to force
  // HTTPS from static entries for the given set of hosts. All hostnames in the
  // bypass list must consist of a single label, i.e. they must be a TLD.
  explicit TransportSecurityState(
      std::vector<std::string> hsts_host_bypass_list);

  TransportSecurityState(const TransportSecurityState&) = delete;
  TransportSecurityState& operator=(const TransportSecurityState&) = delete;

  ~TransportSecurityState();

  // As ShouldUpgradeToSSL(), but also returns whether the decision came from
  // static or dynamic state, for metrics.
  SSLUpgradeDecision GetSSLUpgradeDecision(
      std::string_view host,
      bool is_top_level_nav,
      const NetLogWithSource& net_log = NetLogWithSource());

  // These functions search for static and dynamic STS and PKP states, and
  // invoke the functions of the same name on them. These functions are the
  // primary public interface; direct access to STS and PKP states is best
  // left to tests. The caller needs to handle the optional pinning override
  // when is_issued_by_known_root is false.
  bool ShouldSSLErrorsBeFatal(std::string_view host);
  bool ShouldUpgradeToSSL(std::string_view host,
                          bool is_top_level_nav,
                          const NetLogWithSource& net_log = NetLogWithSource());
  PKPStatus CheckPublicKeyPins(
      std::string_view host,
      bool is_issued_by_known_root,
      const std::vector<SHA256HashValue>& public_key_hashes) const;
  bool HasPublicKeyPins(std::string_view host) const;

  // Returns CT_REQUIREMENTS_NOT_MET if a connection violates CT policy
  // requirements: that is, if a connection to |host|, using the validated
  // certificate |validated_certificate_chain|, is expected to be accompanied
  // with valid Certificate Transparency information that complies with the
  // connection's CTPolicyEnforcer and |policy_compliance| indicates that
  // the connection does not comply.
  //
  // The behavior may be further be altered by setting a RequireCTDelegate
  // via |SetRequireCTDelegate()|.
  //
  // DEPRECATED: CT requirements are enforced inside the cert verifier now.
  // Do not add new uses of this method.
  // TODO(crbug.com/41392053): Remove this and the other CT related stuff from
  // this file.
  ct::CTRequirementsStatus CheckCTRequirements(
      std::string_view host,
      bool is_issued_by_known_root,
      const std::vector<SHA256HashValue>& public_key_hashes,
      const X509Certificate* validated_certificate_chain,
      ct::CTPolicyCompliance policy_compliance);

  // Assign a |Delegate| for persisting the transport security state. If
  // |NULL|, state will not be persisted. The caller retains
  // ownership of |delegate|.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void SetDelegate(Delegate* delegate);

  // Assigns a delegate responsible for determining whether or not a
  // connection to a given host should require Certificate Transparency
  // information that complies with the CT policy provided by a
  // CTPolicyEnforcer.
  // If nullptr, no delegate will be consulted.
  void SetRequireCTDelegate(scoped_refptr<RequireCTDelegate> delegate);

  // If |emergency_disable| is set to true, will stop requiring CT
  // compliance on any further requests regardless of host or certificate
  // status.
  void SetCTEmergencyDisabled(bool emergency_disable) {
    ct_emergency_disable_ = emergency_disable;
  }

  bool is_ct_emergency_disabled_for_testing() const {
    return ct_emergency_disable_;
  }

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
  // |pinsets| should include all known pinsets, |host_pins| the information
  // related to each hostname's pin, and |update_time| the time at which this
  // list was known to be up to date.
  void UpdatePinList(const std::vector<PinSet>& pinsets,
                     const std::vector<PinSetInfo>& host_pins,
                     base::Time update_time);
#endif

  // Clears all dynamic HSTS data.
  //
  // Does NOT persist changes using the Delegate, as this function is only
  // used to clear any dynamic data prior to re-loading it from a file.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void ClearDynamicData();

  // Inserts |state| into |enabled_sts_hosts_| under the key |hashed_host|.
  // |hashed_host| is already in the internal representation.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void AddOrUpdateEnabledSTSHosts(const HashedHost& hashed_host,
                                  const STSState& state);

  // Deletes all dynamic HSTS data created between a time period
  // [|start_time|, |end_time|).
  //
  // If any entries are deleted, the new state will be persisted through
  // the Delegate (if any). Calls |callback| when data is persisted to disk.
  void DeleteAllDynamicDataBetween(base::Time start_time,
                                   base::Time end_time,
                                   base::OnceClosure callback);

  // Deletes any dynamic HSTS data stored for |host|. If |host| doesn't have an
  // exact entry then no action is taken. Does not delete static (i.e.
  // preloaded) data.  Returns true iff an entry was deleted.
  //
  // If an entry is deleted, the new state will be persisted through
  // the Delegate (if any).
  bool DeleteDynamicDataForHost(std::string_view host);

  // Returns true and updates |*result| if |host| has dynamic or static
  // HSTS state. If multiple entries match |host|, dynamic state is preferred
  // over static state and other than that the most specific match determines
  // the return value (both is in deviation of RFC6797, cf.
  // https://crbug.com/821811).
  //
  // Note that the method is not const because it opportunistically removes
  // entries that have expired.
  bool GetSTSState(std::string_view host, STSState* sts_result);

  // Get the PKP state using the current `key_pinning_state_`.
  bool GetPKPState(std::string_view host, PKPState* pkp_result) const;

  // Returns true and updates |*result| iff |host| has static HSTS state. If
  // multiple entries match |host|, the most specific match determines the
  // return value.
  bool GetStaticSTSState(std::string_view host, STSState* sts_result) const;

  // Returns true and updates |*result| iff |host| has dynamic
  // HSTS state. If multiple entries match |host|, the most specific match
  // determines the return value.
  //
  // Note that the method is not const because it opportunistically removes
  // entries that have expired.
  bool GetDynamicSTSState(std::string_view host, STSState* result);

  // Processes an HSTS header value from the host, adding entries to
  // dynamic state if necessary.
  bool AddHSTSHeader(std::string_view host, std::string_view value);

  // Adds explicitly-specified data as if it was processed from an
  // HSTS header (used for net-internals and unit tests).
  void AddHSTS(std::string_view host,
               base::Time expiry,
               bool include_subdomains);

#if BUILDFLAG(CRONET_BUILD)
  // Adds explicitly-specified data as if it was processed from an HPKP header.
  // Note: dynamic PKP data is not persisted.
  void AddHPKP(std::string_view host,
               base::Time expiry,
               bool include_subdomains,
               const HashValueVector& hashes);

  // Enables or disables public key pinning bypass for local trust anchors.
  // Disabling the bypass for local trust anchors is highly discouraged.
  // This method is used by Cronet only and *** MUST NOT *** be used by any
  // other consumer. For more information see "How does key pinning interact
  // with local proxies and filters?" at
  // https://www.chromium.org/Home/chromium-security/security-faq
  void SetEnablePublicKeyPinningBypassForLocalTrustAnchors(bool value);
#endif  //  BUILDFLAG(CRONET_BUILD)

  void AssertCalledOnValidThread() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  // Workaround for ScopedKeyPinningStateChromeDefaults needing to be created
  // before the KeyPinningState is initialized. In some tests it's hard to
  // arrange for that to happen, so this allows a test to reinitialize the
  // key_pinning_state_ on an already created TransportSecurityState object so
  // that it will pick up the new ScopedKeyPinningStateChromeDefaults state.
  // TODO(crbug.com/497882860): remove this.
  void ReinitializePinningForTesting() {
#if BUILDFLAG(CRONET_BUILD)
    key_pinning_state_ = base::MakeRefCounted<KeyPinningStateDynamic>();
#elif BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
    key_pinning_state_ = KeyPinningStateChrome::CreateForCompiledIn();
#else
    key_pinning_state_ = base::MakeRefCounted<KeyPinningStateDummy>();
#endif
  }

  // The number of cached STSState entries.
  size_t num_sts_entries() const;

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
  static base::Time GetBuiltInPinsListTimestamp();
#endif

 private:
  friend class TransportSecurityStateTest;
  friend class TransportSecurityStateStaticFuzzer;
  FRIEND_TEST_ALL_PREFIXES(HttpSecurityHeadersTest, NoClobberPins);

  typedef std::map<HashedHost, STSState> STSStateMap;

  base::DictValue NetLogUpgradeToSSLParam(std::string_view host);

  // IsBuildTimely returns true if the current build is new enough ensure that
  // built in security information (i.e. HSTS preloading and pinning
  // information) is timely.
  static bool IsBuildTimely();

  // If a Delegate is present, notify it that the internal state has
  // changed.
  void DirtyNotify();

  // Adds HSTS state for |host|. The new state supercedes any previous state
  // for the |host|, including static entries.
  //
  // The new state for |host| is persisted using the Delegate (if any).
  void AddHSTSInternal(std::string_view host,
                       STSState::UpgradeMode upgrade_mode,
                       base::Time expiry,
                       bool include_subdomains);

  // The sets of hosts that have enabled TransportSecurity. |domain| will always
  // be empty for a STSState in this map; the domain comes from the map keys
  // instead. In addition, |upgrade_mode| in the STSState is never MODE_DEFAULT.
  STSStateMap enabled_sts_hosts_;

  raw_ptr<Delegate> delegate_ = nullptr;

  scoped_refptr<RequireCTDelegate> require_ct_delegate_;

  std::set<std::string, std::less<>> hsts_host_bypass_list_;

  bool ct_emergency_disable_ = false;

  // TODO(crbug.com/497882860): remove key pinning from TransportSecurityState.
#if BUILDFLAG(CRONET_BUILD)
  scoped_refptr<KeyPinningStateDynamic> key_pinning_state_ =
      base::MakeRefCounted<KeyPinningStateDynamic>();
#elif BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
  scoped_refptr<KeyPinningState> key_pinning_state_ =
      KeyPinningStateChrome::CreateForCompiledIn();
#else
  scoped_refptr<KeyPinningState> key_pinning_state_ =
      base::MakeRefCounted<KeyPinningStateDummy>();
#endif

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_STATE_H_
