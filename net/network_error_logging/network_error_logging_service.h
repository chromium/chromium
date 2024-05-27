// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_SERVICE_H_
#define NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Value;
}  // namespace base

namespace net {
class ReportingService;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace features {
extern const base::Feature NET_EXPORT kNetworkErrorLogging;
}  // namespace features

namespace net {

class NET_EXPORT NetworkErrorLoggingService {
 public:
  class PersistentNelStore;

  // Every (NAK, origin) pair can have at most one policy.
  struct NET_EXPORT NelPolicyKey {
    NelPolicyKey();
    NelPolicyKey(const NetworkAnonymizationKey& network_anonymization_key,
                 const url::Origin& origin);
    NelPolicyKey(const NelPolicyKey& other);
    ~NelPolicyKey();

    bool operator<(const NelPolicyKey& other) const;
    bool operator==(const NelPolicyKey& other) const;
    bool operator!=(const NelPolicyKey& other) const;

    // The NAK of the request this policy was received from. This will be used
    // for any requests uploading reports according to this policy. (Not
    // included in the report itself.)
    NetworkAnonymizationKey network_anonymization_key;

    url::Origin origin;
  };

  // Used for wildcard policies that are applicable to |domain| and its
  // subdomains.
  struct WildcardNelPolicyKey {
    WildcardNelPolicyKey();
    WildcardNelPolicyKey(
        const NetworkAnonymizationKey& network_anonymization_key,
        const std::string& domain);
    explicit WildcardNelPolicyKey(const NelPolicyKey& origin_key);
    WildcardNelPolicyKey(const WildcardNelPolicyKey& other);
    ~WildcardNelPolicyKey();

    bool operator<(const WildcardNelPolicyKey& other) const;

    // The NAK of the request this policy was received from. This will be used
    // for any requests uploading reports according to this policy. (Not
    // included in the report itself.)
    NetworkAnonymizationKey network_anonymization_key;

    std::string domain;
  };

  // NEL policy set by an origin.
  struct NET_EXPORT NelPolicy {
    NelPolicy();
    NelPolicy(const NelPolicy& other);
    ~NelPolicy();

    NelPolicyKey key;

    IPAddress received_ip_address = IPAddress();

    // Reporting API endpoint group to which reports should be sent.
    std::string report_to;

    base::Time expires;

    double success_fraction = 0.0;
    double failure_fraction = 1.0;
    bool include_subdomains = false;

    // Last time the policy was accessed to create a report, even if no report
    // ends up being queued. Also updated when the policy is first set.
    mutable base::Time last_used;
  };

  // The details of a network error that are included in an NEL report.
  //
  // See http://wicg.github.io/network-error-logging/#dfn-network-error-object
  // for details on the semantics of each field.
  struct NET_EXPORT RequestDetails {
    RequestDetails();
    RequestDetails(const RequestDetails& other);
    ~RequestDetails();

    // NetworkAnonymizationKey of the request triggering the error. Not included
    // in the uploaded report.
    NetworkAnonymizationKey network_anonymization_key;

    GURL uri;
    GURL referrer;
    std::string user_agent;
    IPAddress server_ip;
    std::string protocol;
    std::string method;
    int status_code;
    base::TimeDelta elapsed_time;
    Error type;

    // Upload nesting depth of this request.
    //
    // If the request is not a Reporting upload, the depth is 0.
    //
    // If the request is a Reporting upload, the depth is the max of the depth
    // of the requests reported within it plus 1. (Non-NEL reports are
    // considered to have depth 0.)
    int reporting_upload_depth;
  };

  // The details of a signed exchange report.
  struct NET_EXPORT SignedExchangeReportDetails {
    SignedExchangeReportDetails();
    SignedExchangeReportDetails(const SignedExchangeReportDetails& other);
    ~SignedExchangeReportDetails();

    // NetworkAnonymizationKey of the request triggering the error. Not included
    // in the uploaded report.
    NetworkAnonymizationKey network_anonymization_key;

    bool success;
    std::string type;
    GURL outer_url;
    GURL inner_url;
    GURL cert_url;
    std::string referrer;
    IPAddress server_ip_address;
    std::string protocol;
    std::string method;
    int32_t status_code;
    base::TimeDelta elapsed_time;
    std::string user_agent;
  };

  static const char kHeaderName[];

  static const char kReportType[];

  static const int kMaxNestedReportDepth;

  // Keys for data included in report bodies. Exposed for tests.

  static const char kReferrerKey[];
  static const char kSamplingFractionKey[];
  static const char kServerIpKey[];
  static const char kProtocolKey[];
  static const char kMethodKey[];
  static const char kStatusCodeKey[];
  static const char kElapsedTimeKey[];
  static const char kPhaseKey[];
  static const char kTypeKey[];

  static const char kSignedExchangePhaseValue[];
  static const char kSignedExchangeBodyKey[];
  static const char kOuterUrlKey[];
  static const char kInnerUrlKey[];
  static const char kCertUrlKey[];

  // Maximum number of NEL policies to store before evicting.
  static const size_t kMaxPolicies;

  static const char kSignedExchangeRequestOutcomeHistogram[];

  // Used for histogramming Signed Exchange request outcomes only. Previously,
  // the outcome of all requests would be histogrammed, but this was removed in
  // crbug.com/1007122 because the histogram was very large and not very useful.
  enum class RequestOutcome {
    kDiscardedNoNetworkErrorLoggingService = 0,

    kDiscardedNoReportingService = 1,
    kDiscardedInsecureOrigin = 2,
    kDiscardedNoOriginPolicy = 3,
    kDiscardedUnmappedError = 4,
    kDiscardedReportingUpload = 5,
    kDiscardedUnsampledSuccess = 6,
    kDiscardedUnsampledFailure = 7,
    kQueued = 8,
    kDiscardedNonDNSSubdomainReport = 9,
    kDiscardedIPAddressMismatch = 10,

    kMaxValue = kDiscardedIPAddressMismatch
  };

  // NEL policies are persisted to disk if |store| is not null.
  // The store, if given, should outlive |*this|.
  static std::unique_ptr<NetworkErrorLoggingService> Create(
      PersistentNelStore* store);

  NetworkErrorLoggingService(const NetworkErrorLoggingService&) = delete;
  NetworkErrorLoggingService& operator=(const NetworkErrorLoggingService&) =
      delete;

  virtual ~NetworkErrorLoggingService();

  // Ingests a "NEL:" header received for |network_anonymization_key| and
  // |origin| from |received_ip_address| with normalized value |value|. May or
  // may not actually set a policy for that origin.
  virtual void OnHeader(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      const IPAddress& received_ip_address,
      const std::string& value) = 0;

  // Considers queueing a network error report for the request described in
  // |details|.  The contents of |details| might be changed, depending on the
  // NEL policy associated with the request's origin.  Note that |details| is
  // passed by value, so that it doesn't need to be copied in this function if
  // it needs to be changed.  Consider using std::move to pass this parameter if
  // the caller doesn't need to access it after this method call.
  //
  // Note that Network Error Logging can report a fraction of successful
  // requests as well (to calculate error rates), so this should be called on
  // *all* secure requests. NEL is only available to secure origins, so this is
  // not called on any insecure requests.
  virtual void OnRequest(RequestDetails details) = 0;

  // Queues a Signed Exchange report.
  virtual void QueueSignedExchangeReport(
      SignedExchangeReportDetails details) = 0;

  // Removes browsing data (origin policies) associated with any origin for
  // which |origin_filter| returns true.
  virtual void RemoveBrowsingData(
      const base::RepeatingCallback<bool(const url::Origin&)>&
          origin_filter) = 0;

  // Removes browsing data (origin policies) for all origins. Allows slight
  // optimization over passing an always-true filter to RemoveBrowsingData.
  virtual void RemoveAllBrowsingData() = 0;

  // Sets the ReportingService that will be used to queue network error reports.
  // If |nullptr| is passed, reports will be queued locally or discarded.
  // |reporting_service| must outlive the NetworkErrorLoggingService.
  // Should not be called again if previously called with a non-null pointer.
  void SetReportingService(ReportingService* reporting_service);

  // Shuts down the NEL service, so that no more requests or headers can be
  // processed, no more reports are queued, and browsing data can no longer be
  // cleared.
  void OnShutdown();

  // Sets a base::Clock (used to track policy expiration) for tests.
  // |clock| must outlive the NetworkErrorLoggingService, and cannot be
  // nullptr.
  void SetClockForTesting(const base::Clock* clock);

  // Dumps info about all the currently stored policies, including expired ones.
  // Used to display information about NEL policies on the NetLog Reporting tab.
  virtual base::Value StatusAsValue() const;

  // Gets the (NAK, origin) keys of all currently stored policies, including
  // expired ones.
  virtual std::set<NelPolicyKey> GetPolicyKeysForTesting();

  virtual PersistentNelStore* GetPersistentNelStoreForTesting();
  virtual ReportingService* GetReportingServiceForTesting();

 protected:
  NetworkErrorLoggingService();

  // Unowned:
  raw_ptr<const base::Clock> clock_;
  raw_ptr<ReportingService> reporting_service_ = nullptr;
  bool shut_down_ = false;
};

// Persistent storage for NEL policies.
class NET_EXPORT NetworkErrorLoggingService::PersistentNelStore {
 public:
  using NelPoliciesLoadedCallback =
      base::OnceCallback<void(std::vector<NelPolicy>)>;

  PersistentNelStore() = default;

  PersistentNelStore(const PersistentNelStore&) = delete;
  PersistentNelStore& operator=(const PersistentNelStore&) = delete;

  virtual ~PersistentNelStore() = default;

  // Initializes the store and retrieves stored NEL policies. This will be
  // called only once at startup.
  virtual void LoadNelPolicies(NelPoliciesLoadedCallback loaded_callback) = 0;

  // Adds a NEL policy to the store.
  virtual void AddNelPolicy(const NelPolicy& policy) = 0;

  // Updates the access time of the NEL policy in the store.
  virtual void UpdateNelPolicyAccessTime(const NelPolicy& policy) = 0;

  // Deletes a NEL policy from the store.
  virtual void DeleteNelPolicy(const NelPolicy& policy) = 0;

  // Flushes the store.
  virtual void Flush() = 0;
};

}  // namespace net

#endif  // NET_NETWORK_ERROR_LOGGING_NETWORK_ERROR_LOGGING_SERVICE_H_
