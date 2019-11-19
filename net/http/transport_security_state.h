// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_TRANSPORT_SECURITY_STATE_H_
#define NET_HTTP_TRANSPORT_SECURITY_STATE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/expiring_cache.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/transport_security_state_source.h"
#include "url/gurl.h"

namespace net {

namespace ct {
enum class CTPolicyCompliance;
}

class HostPortPair;
class SSLInfo;
class X509Certificate;

// Controls whether or not Certificate Transparency should be enforced for
// newly-issued certificates.
extern const NET_EXPORT_PRIVATE base::Feature kEnforceCTForNewCerts;

void NET_EXPORT_PRIVATE SetTransportSecurityStateSourceForTesting(
    const TransportSecurityStateSource* source);

// Tracks which hosts have enabled strict transport security and/or public
// key pins.
//
// This object manages the in-memory store. Register a Delegate with
// |SetDelegate| to persist the state to disk.
//
// HTTP strict transport security (HSTS) is defined in
// http://tools.ietf.org/html/ietf-websec-strict-transport-sec.
class NET_EXPORT TransportSecurityState {
 public:
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
    virtual ~Delegate() {}
  };

  class NET_EXPORT RequireCTDelegate {
   public:
    // Provides a capability for altering the default handling of Certificate
    // Transparency information, allowing it to be always required for some
    // hosts, for some hosts to be opted out of the default policy, or
    // allowing the TransportSecurityState to apply the default security
    // policies.
    enum class CTRequirementLevel {
      // The host is required to always supply Certificate Transparency
      // information that complies with the CT policy.
      REQUIRED,

      // The host is explicitly not required to supply Certificate
      // Transparency information that complies with the CT policy.
      NOT_REQUIRED,

      // The delegate makes no statements, positive or negative, about
      // requiring the host to supply Certificate Transparency information,
      // allowing the default behaviour to happen.
      DEFAULT,
    };

    // Called by the TransportSecurityState, allows the Delegate to override
    // the default handling of Certificate Transparency requirements, if
    // desired.
    // |hostname| contains the host being contacted, serving the certificate
    // |chain|, with the set of hashesh |hashes|. Note that |hashes| and
    // |chain| are not guaranteed to be in the same order - that is, the first
    // hash in |hashes| is NOT guaranteed to be for the leaf cert in |chain|.
    virtual CTRequirementLevel IsCTRequiredForHost(
        const std::string& hostname,
        const X509Certificate* chain,
        const HashValueVector& hashes) = 0;

   protected:
    virtual ~RequireCTDelegate() = default;
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

    UpgradeMode upgrade_mode;

    // Are subdomains subject to this policy state?
    bool include_subdomains;

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
    const std::string& hostname() const { return iterator_->first; }
    const STSState& domain_state() const { return iterator_->second; }

   private:
    std::map<std::string, STSState>::const_iterator iterator_;
    std::map<std::string, STSState>::const_iterator end_;
  };

  // PKPStatus describes the result of a pinning check.
  enum class PKPStatus {
    // Pinning was enabled and the necessary pins were not present.
    VIOLATED,

    // Pinning was not enabled, or pinning was enabled and the certificate
    // satisfied the pins.
    OK,

    // Pinning was enabled and the certificate did not satisfy the pins, but the
    // violation was ignored due to local policy, such as a local trust anchor.
    BYPASSED,
  };

  // A PKPState describes the public key pinning state.
  class NET_EXPORT PKPState {
   public:
    PKPState();
    PKPState(const PKPState& other);
    ~PKPState();

    // The absolute time (UTC) when the |spki_hashes| (and other state) were
    // observed.
    base::Time last_observed;

    // The absolute time (UTC) when the |spki_hashes| expire.
    base::Time expiry;

    // Optional; hashes of pinned SubjectPublicKeyInfos.
    HashValueVector spki_hashes;

    // Optional; hashes of static known-bad SubjectPublicKeyInfos which MUST
    // NOT intersect with the set of SPKIs in the TLS server's certificate
    // chain.
    HashValueVector bad_spki_hashes;

    // Are subdomains subject to this policy state?
    bool include_subdomains;

    // The domain which matched during a search for this DomainState entry.
    // Updated by |GetDynamicPKPState| and |GetStaticDomainState|.
    std::string domain;

    // An optional URI indicating where reports should be sent when this
    // pin is violated, or empty when omitted.
    GURL report_uri;

    // Takes a set of SubjectPublicKeyInfo |hashes| and returns true if:
    //   1) |bad_static_spki_hashes| does not intersect |hashes|; AND
    //   2) Both |static_spki_hashes| and |dynamic_spki_hashes| are empty
    //      or at least one of them intersects |hashes|.
    //
    // |{dynamic,static}_spki_hashes| contain trustworthy public key hashes,
    // any one of which is sufficient to validate the certificate chain in
    // question. The public keys could be of a root CA, intermediate CA, or
    // leaf certificate, depending on the security vs. disaster recovery
    // tradeoff selected. (Pinning only to leaf certifiates increases
    // security because you no longer trust any CAs, but it hampers disaster
    // recovery because you can't just get a new certificate signed by the
    // CA.)
    //
    // |bad_static_spki_hashes| contains public keys that we don't want to
    // trust.
    bool CheckPublicKeyPins(const HashValueVector& hashes,
                            std::string* failure_log) const;

    // Returns true if any of the HashValueVectors |static_spki_hashes|,
    // |bad_static_spki_hashes|, or |dynamic_spki_hashes| contains any
    // items.
    bool HasPublicKeyPins() const;
  };

  // An ExpectCTState describes a site that expects valid Certificate
  // Transparency information to be supplied on every connection to it.
  class NET_EXPORT ExpectCTState {
   public:
    ExpectCTState();
    ~ExpectCTState();

    // The domain which matched during a search for this DomainState entry.
    std::string domain;
    // The URI to which reports should be sent if valid CT info is not
    // provided.
    GURL report_uri;
    // True if connections should be closed if they do not comply with the CT
    // policy. If false, noncompliant connections will be allowed but reports
    // will be sent about the violation.
    bool enforce;
    // The absolute time (UTC) when the Expect-CT state was last observed.
    base::Time last_observed;
    // The absolute time (UTC) when the Expect-CT state expires.
    base::Time expiry;
  };

  class NET_EXPORT ExpectCTStateIterator {
   public:
    explicit ExpectCTStateIterator(const TransportSecurityState& state);
    ~ExpectCTStateIterator();

    bool HasNext() const { return iterator_ != end_; }
    void Advance() { ++iterator_; }
    const std::string& hostname() const { return iterator_->first; }
    const ExpectCTState& domain_state() const { return iterator_->second; }

   private:
    std::map<std::string, ExpectCTState>::const_iterator iterator_;
    std::map<std::string, ExpectCTState>::const_iterator end_;
  };

  // An interface for asynchronously sending HPKP violation reports.
  class NET_EXPORT ReportSenderInterface {
   public:
    // Sends the given serialized |report| to |report_uri| with
    // Content-Type header as specified in
    // |content_type|. |content_type| should be non-empty.
    // |success_callback| is called iff an HTTP 200 response is received.
    // |error_callback| is called in all other cases. Error callback's
    // |net_error| can be net::OK if the upload was successful but the server
    // returned a non-HTTP 200 |http_response_code|. In all other cases,
    // error callback's |http_response_code| is -1.
    virtual void Send(const GURL& report_uri,
                      base::StringPiece content_type,
                      base::StringPiece report,
                      const base::Callback<void()>& success_callback,
                      const base::Callback<void(const GURL&,
                                                int /* net_error */,
                                                int /* http_response_code */)>&
                          error_callback) = 0;

   protected:
    virtual ~ReportSenderInterface() {}
  };

  // An interface for building and asynchronously sending reports when a
  // site expects valid Certificate Transparency information but it
  // wasn't supplied.
  class NET_EXPORT ExpectCTReporter {
   public:
    // Called when the host in |host_port_pair| has opted in to have
    // reports about Expect CT policy violations sent to |report_uri|,
    // and such a violation has occurred.
    virtual void OnExpectCTFailed(
        const net::HostPortPair& host_port_pair,
        const GURL& report_uri,
        base::Time expiration,
        const X509Certificate* validated_certificate_chain,
        const X509Certificate* served_certificate_chain,
        const SignedCertificateTimestampAndStatusList&
            signed_certificate_timestamps) = 0;

   protected:
    virtual ~ExpectCTReporter() {}
  };

  // Indicates whether or not a public key pin check should send a
  // report if a violation is detected.
  enum PublicKeyPinReportStatus { ENABLE_PIN_REPORTS, DISABLE_PIN_REPORTS };

  // Indicates whether or not an Expect-CT check should send a report if a
  // violation is detected.
  enum ExpectCTReportStatus {
    ENABLE_EXPECT_CT_REPORTS,
    DISABLE_EXPECT_CT_REPORTS
  };

  // Indicates whether a connection met CT requirements.
  enum CTRequirementsStatus {
    // CT was not required for the connection.
    CT_NOT_REQUIRED,
    // CT was required for the connection and valid Certificate Transparency
    // information was provided.
    CT_REQUIREMENTS_MET,
    // CT was required for the connection but valid CT info was not provided.
    CT_REQUIREMENTS_NOT_MET,
  };

  // Feature that controls whether Expect-CT HTTP headers are parsed, processed,
  // and stored.
  static const base::Feature kDynamicExpectCTFeature;

  TransportSecurityState();

  // Creates a TransportSecurityState object that will skip the check to force
  // HTTPS from static entries for the given set of hosts. All hostnames in the
  // bypass list must consist of a single label, i.e. they must be a TLD.
  explicit TransportSecurityState(
      std::vector<std::string> hsts_host_bypass_list);

  ~TransportSecurityState();

  // These functions search for static and dynamic STS and PKP states, and
  // invoke the functions of the same name on them. These functions are the
  // primary public interface; direct access to STS and PKP states is best
  // left to tests. The caller needs to handle the optional pinning override
  // when is_issued_by_known_root is false.
  bool ShouldSSLErrorsBeFatal(const std::string& host);
  bool ShouldUpgradeToSSL(const std::string& host);
  PKPStatus CheckPublicKeyPins(
      const HostPortPair& host_port_pair,
      bool is_issued_by_known_root,
      const HashValueVector& hashes,
      const X509Certificate* served_certificate_chain,
      const X509Certificate* validated_certificate_chain,
      const PublicKeyPinReportStatus report_status,
      std::string* failure_log);
  bool HasPublicKeyPins(const std::string& host);

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
  // This method checks Expect-CT state for |host| if |issued_by_known_root| is
  // true. If Expect-CT is configured for |host| and the connection is not
  // compliant and |report_status| is ENABLE_EXPECT_CT_REPORTS, then a report
  // will be sent.
  CTRequirementsStatus CheckCTRequirements(
      const net::HostPortPair& host_port_pair,
      bool is_issued_by_known_root,
      const HashValueVector& public_key_hashes,
      const X509Certificate* validated_certificate_chain,
      const X509Certificate* served_certificate_chain,
      const SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps,
      const ExpectCTReportStatus report_status,
      ct::CTPolicyCompliance policy_compliance);

  // Assign a |Delegate| for persisting the transport security state. If
  // |NULL|, state will not be persisted. The caller retains
  // ownership of |delegate|.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void SetDelegate(Delegate* delegate);

  void SetReportSender(ReportSenderInterface* report_sender);

  void SetExpectCTReporter(ExpectCTReporter* expect_ct_reporter);

  // Assigns a delegate responsible for determining whether or not a
  // connection to a given host should require Certificate Transparency
  // information that complies with the CT policy provided by a
  // CTPolicyEnforcer.
  // If nullptr, no delegate will be consulted.
  // The caller retains ownership of the |delegate|, and must persist for
  // the lifetime of this object or until called with nullptr, whichever
  // occurs first.
  void SetRequireCTDelegate(RequireCTDelegate* delegate);

  // Clears all dynamic data (e.g. HSTS and HPKP data).
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
  void AddOrUpdateEnabledSTSHosts(const std::string& hashed_host,
                                  const STSState& state);

  // Inserts |state| into |enabled_expect_ct_hosts_| under the key
  // |hashed_host|. |hashed_host| is already in the internal representation.
  // Note: This is only used for serializing/deserializing the
  // TransportSecurityState.
  void AddOrUpdateEnabledExpectCTHosts(const std::string& hashed_host,
                                       const ExpectCTState& state);

  // Deletes all dynamic data (e.g. HSTS or HPKP data) created since a given
  // time.
  //
  // If any entries are deleted, the new state will be persisted through
  // the Delegate (if any). Calls |callback| when data is persisted to disk.
  void DeleteAllDynamicDataSince(const base::Time& time,
                                 base::OnceClosure callback);

  // Deletes any dynamic data stored for |host| (e.g. HSTS or HPKP data).
  // If |host| doesn't have an exact entry then no action is taken. Does
  // not delete static (i.e. preloaded) data.  Returns true iff an entry
  // was deleted.
  //
  // If an entry is deleted, the new state will be persisted through
  // the Delegate (if any).
  bool DeleteDynamicDataForHost(const std::string& host);

  // Returns true and updates |*result| if |host| has dynamic or static
  // HSTS/HPKP (respectively) state. If multiple entries match |host|, dynamic
  // state is preferred over static state and other than that the most specific
  // match determines the return value (both is in deviation of RFC6797, cf.
  // https://crbug.com/821811).
  //
  // Note that these methods are not const because they opportunistically remove
  // entries that have expired.
  bool GetSTSState(const std::string& host, STSState* result);
  bool GetPKPState(const std::string& host, PKPState* result);

  // Returns true and updates |*sts_result| and/or |*pkp_result| if there is
  // static (built-in) state for |host|. If multiple entries match |host|,
  // the most specific match determines the return value.
  bool GetStaticDomainState(const std::string& host,
                            STSState* sts_result,
                            PKPState* pkp_result) const;

  // Returns true and updates |*result| iff |host| has dynamic
  // HSTS/HPKP/Expect-CT (respectively) state. If multiple entries match |host|,
  // the most specific match determines the return value.
  //
  // Note that these methods are not const because they opportunistically remove
  // entries that have expired.
  bool GetDynamicSTSState(const std::string& host, STSState* result);
  bool GetDynamicPKPState(const std::string& host, PKPState* result);
  bool GetDynamicExpectCTState(const std::string& host, ExpectCTState* result);

  // Processes an HSTS header value from the host, adding entries to
  // dynamic state if necessary.
  bool AddHSTSHeader(const std::string& host, const std::string& value);

  // Adds explicitly-specified data as if it was processed from an
  // HSTS header (used for net-internals and unit tests).
  void AddHSTS(const std::string& host,
               const base::Time& expiry,
               bool include_subdomains);

  // Adds explicitly-specified data as if it was processed from an HPKP header.
  // Note: dynamic PKP data is not persisted.
  void AddHPKP(const std::string& host,
               const base::Time& expiry,
               bool include_subdomains,
               const HashValueVector& hashes,
               const GURL& report_uri);

  // Adds explicitly-specified data as if it was processed from an Expect-CT
  // header.
  // Note: This method will persist the Expect-CT data if a Delegate is present.
  //       Make sure that the delegate is nullptr if the persistence is not
  //       desired. See |SetDelegate| method for more details.
  void AddExpectCT(const std::string& host,
                   const base::Time& expiry,
                   bool enforce,
                   const GURL& report_uri);

  // Enables or disables public key pinning bypass for local trust anchors.
  // Disabling the bypass for local trust anchors is highly discouraged.
  // This method is used by Cronet only and *** MUST NOT *** be used by any
  // other consumer. For more information see "How does key pinning interact
  // with local proxies and filters?" at
  // https://www.chromium.org/Home/chromium-security/security-faq
  void SetEnablePublicKeyPinningBypassForLocalTrustAnchors(bool value);

  // Parses |value| as a Expect CT header value. If valid and served on a
  // CT-compliant connection, adds an entry to the dynamic state. If valid but
  // not served on a CT-compliant connection, a report is sent to alert the site
  // owner of the misconfiguration (provided that a reporter has been set via
  // SetExpectCTReporter).
  //
  // The header can also have the value "preload", indicating that the site
  // wants to opt-in to the static report-only version of Expect-CT. If the
  // given host is present on the preload list and the build is timely and the
  // connection is not CT-compliant, then a report will be sent.
  void ProcessExpectCTHeader(const std::string& value,
                             const HostPortPair& host_port_pair,
                             const SSLInfo& ssl_info);

  void AssertCalledOnValidThread() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  // For unit tests only. Causes CheckCTRequirements() to return
  // CT_REQUIREMENTS_NOT_MET (if |*required| is true) or CT_REQUIREMENTS_MET (if
  // |*required| is false) for non-compliant connections by default (that is,
  // unless a RequireCTDelegate overrides). Set to nullptr to reset.
  static void SetShouldRequireCTForTesting(bool* required);

  // For unit tests only. Clears the caches that deduplicate sent PKP and
  // Expect-CT reports.
  void ClearReportCachesForTesting();

  // For unit tests only.
  void EnableStaticPinsForTesting() { enable_static_pins_ = true; }
  bool has_dynamic_pkp_state() const { return !enabled_pkp_hosts_.empty(); }

 private:
  friend class TransportSecurityStateTest;
  friend class TransportSecurityStateStaticFuzzer;
  FRIEND_TEST_ALL_PREFIXES(HttpSecurityHeadersTest, NoClobberPins);
  FRIEND_TEST_ALL_PREFIXES(URLRequestTestHTTP, PreloadExpectCTHeader);

  typedef std::map<std::string, STSState> STSStateMap;
  typedef std::map<std::string, PKPState> PKPStateMap;
  typedef std::map<std::string, ExpectCTState> ExpectCTStateMap;
  typedef ExpiringCache<std::string,
                        bool,
                        base::TimeTicks,
                        std::less<base::TimeTicks>>
      ReportCache;

  // IsBuildTimely returns true if the current build is new enough ensure that
  // built in security information (i.e. HSTS preloading and pinning
  // information) is timely.
  static bool IsBuildTimely();

  // Helper method for actually checking pins.
  PKPStatus CheckPublicKeyPinsImpl(
      const HostPortPair& host_port_pair,
      bool is_issued_by_known_root,
      const HashValueVector& hashes,
      const X509Certificate* served_certificate_chain,
      const X509Certificate* validated_certificate_chain,
      const PublicKeyPinReportStatus report_status,
      std::string* failure_log);

  // If a Delegate is present, notify it that the internal state has
  // changed.
  void DirtyNotify();

  // Adds HSTS state to |host|.
  void AddHSTSInternal(const std::string& host,
                       STSState::UpgradeMode upgrade_mode,
                       const base::Time& expiry,
                       bool include_subdomains);

  // Adds HPKP state to |host|.
  void AddHPKPInternal(const std::string& host,
                       const base::Time& last_observed,
                       const base::Time& expiry,
                       bool include_subdomains,
                       const HashValueVector& hashes,
                       const GURL& report_uri);

  // Adds Expect-CT state to |host|.
  void AddExpectCTInternal(const std::string& host,
                           const base::Time& last_observed,
                           const base::Time& expiry,
                           bool enforce,
                           const GURL& report_uri);

  // Enable TransportSecurity for |host|. |state| supercedes any previous
  // state for the |host|, including static entries.
  //
  // The new state for |host| is persisted using the Delegate (if any).
  void EnableSTSHost(const std::string& host, const STSState& state);
  void EnablePKPHost(const std::string& host, const PKPState& state);
  void EnableExpectCTHost(const std::string& host, const ExpectCTState& state);

  // Returns true if a request to |host_port_pair| with the given
  // SubjectPublicKeyInfo |hashes| satisfies the pins in |pkp_state|,
  // and false otherwise. If a violation is found and reporting is
  // configured (i.e. there is a report URI in |pkp_state| and
  // |report_status| says to), this method sends an HPKP violation
  // report containing |served_certificate_chain| and
  // |validated_certificate_chain|.
  PKPStatus CheckPinsAndMaybeSendReport(
      const HostPortPair& host_port_pair,
      bool is_issued_by_known_root,
      const TransportSecurityState::PKPState& pkp_state,
      const HashValueVector& hashes,
      const X509Certificate* served_certificate_chain,
      const X509Certificate* validated_certificate_chain,
      const TransportSecurityState::PublicKeyPinReportStatus report_status,
      std::string* failure_log);

  // Returns true and updates |*expect_ct_result| iff there is a static
  // (built-in) state for |host| with expect_ct=true.
  bool GetStaticExpectCTState(const std::string& host,
                              ExpectCTState* expect_ct_result) const;

  void MaybeNotifyExpectCTFailed(
      const HostPortPair& host_port_pair,
      const GURL& report_uri,
      base::Time expiration,
      const X509Certificate* validated_certificate_chain,
      const X509Certificate* served_certificate_chain,
      const SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps);

  // The sets of hosts that have enabled TransportSecurity. |domain| will always
  // be empty for a STSState, PKPState, or ExpectCTState in these maps; the
  // domain comes from the map keys instead. In addition, |upgrade_mode| in the
  // STSState is never MODE_DEFAULT and |HasPublicKeyPins| in the PKPState
  // always returns true.
  STSStateMap enabled_sts_hosts_;
  PKPStateMap enabled_pkp_hosts_;
  ExpectCTStateMap enabled_expect_ct_hosts_;

  Delegate* delegate_ = nullptr;

  ReportSenderInterface* report_sender_ = nullptr;

  // True if static pins should be used.
  bool enable_static_pins_;

  // True if static expect-CT state should be used.
  bool enable_static_expect_ct_;

  // True if public key pinning bypass is enabled for local trust anchors.
  bool enable_pkp_bypass_for_local_trust_anchors_;

  ExpectCTReporter* expect_ct_reporter_ = nullptr;

  RequireCTDelegate* require_ct_delegate_ = nullptr;

  // Keeps track of reports that have been sent recently for
  // rate-limiting.
  ReportCache sent_hpkp_reports_cache_;
  ReportCache sent_expect_ct_reports_cache_;

  std::set<std::string> hsts_host_bypass_list_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(TransportSecurityState);
};

}  // namespace net

#endif  // NET_HTTP_TRANSPORT_SECURITY_STATE_H_
