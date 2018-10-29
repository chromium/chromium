// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/network_error_logging/network_error_logging_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/network_error_logging/network_error_logging_delegate.h"
#include "net/reporting/reporting_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

const int kMaxJsonSize = 16 * 1024;
const int kMaxJsonDepth = 4;

const char kReportToKey[] = "report_to";
const char kMaxAgeKey[] = "max_age";
const char kIncludeSubdomainsKey[] = "include_subdomains";
const char kSuccessFractionKey[] = "success_fraction";
const char kFailureFractionKey[] = "failure_fraction";

// Returns the superdomain of a given domain, or the empty string if the given
// domain is just a single label. Note that this does not take into account
// anything like the Public Suffix List, so the superdomain may end up being a
// bare TLD.
//
// Examples:
//
// GetSuperdomain("assets.example.com") -> "example.com"
// GetSuperdomain("example.net") -> "net"
// GetSuperdomain("littlebox") -> ""
//
// TODO(juliatuttle): Deduplicate from Reporting in //net.
std::string GetSuperdomain(const std::string& domain) {
  size_t dot_pos = domain.find('.');
  if (dot_pos == std::string::npos)
    return "";

  return domain.substr(dot_pos + 1);
}

const char kApplicationPhase[] = "application";
const char kConnectionPhase[] = "connection";
const char kDnsPhase[] = "dns";

const char kDnsAddressChangedType[] = "dns.address_changed";
const char kHttpErrorType[] = "http.error";

const struct {
  Error error;
  const char* phase;
  const char* type;
} kErrorTypes[] = {
    {OK, kApplicationPhase, "ok"},

    // dns.unreachable?
    {ERR_NAME_NOT_RESOLVED, kDnsPhase, "dns.name_not_resolved"},
    {ERR_NAME_RESOLUTION_FAILED, kDnsPhase, "dns.failed"},
    {ERR_DNS_TIMED_OUT, kDnsPhase, "dns.timed_out"},

    {ERR_CONNECTION_TIMED_OUT, kConnectionPhase, "tcp.timed_out"},
    {ERR_CONNECTION_CLOSED, kConnectionPhase, "tcp.closed"},
    {ERR_CONNECTION_RESET, kConnectionPhase, "tcp.reset"},
    {ERR_CONNECTION_REFUSED, kConnectionPhase, "tcp.refused"},
    {ERR_CONNECTION_ABORTED, kConnectionPhase, "tcp.aborted"},
    {ERR_ADDRESS_INVALID, kConnectionPhase, "tcp.address_invalid"},
    {ERR_ADDRESS_UNREACHABLE, kConnectionPhase, "tcp.address_unreachable"},
    {ERR_CONNECTION_FAILED, kConnectionPhase, "tcp.failed"},

    {ERR_SSL_VERSION_OR_CIPHER_MISMATCH, kConnectionPhase,
     "tls.version_or_cipher_mismatch"},
    {ERR_BAD_SSL_CLIENT_AUTH_CERT, kConnectionPhase,
     "tls.bad_client_auth_cert"},
    {ERR_CERT_COMMON_NAME_INVALID, kConnectionPhase, "tls.cert.name_invalid"},
    {ERR_CERT_DATE_INVALID, kConnectionPhase, "tls.cert.date_invalid"},
    {ERR_CERT_AUTHORITY_INVALID, kConnectionPhase,
     "tls.cert.authority_invalid"},
    {ERR_CERT_INVALID, kConnectionPhase, "tls.cert.invalid"},
    {ERR_CERT_REVOKED, kConnectionPhase, "tls.cert.revoked"},
    {ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN, kConnectionPhase,
     "tls.cert.pinned_key_not_in_cert_chain"},
    {ERR_SSL_PROTOCOL_ERROR, kConnectionPhase, "tls.protocol.error"},
    // tls.failed?

    // http.protocol.error?
    {ERR_INVALID_HTTP_RESPONSE, kApplicationPhase, "http.response.invalid"},
    {ERR_TOO_MANY_REDIRECTS, kApplicationPhase, "http.response.redirect_loop"},
    {ERR_EMPTY_RESPONSE, kApplicationPhase, "http.response.empty"},
    // http.failed?

    {ERR_ABORTED, kApplicationPhase, "abandoned"},
    // unknown?

    // TODO(juliatuttle): Surely there are more errors we want here.
};

bool GetPhaseAndTypeFromNetError(Error error,
                                 std::string* phase_out,
                                 std::string* type_out) {
  for (size_t i = 0; i < arraysize(kErrorTypes); ++i) {
    if (kErrorTypes[i].error == error) {
      *phase_out = kErrorTypes[i].phase;
      *type_out = kErrorTypes[i].type;
      return true;
    }
  }
  return false;
}

bool IsHttpError(const NetworkErrorLoggingService::RequestDetails& request) {
  return request.status_code >= 400 && request.status_code < 600;
}

enum class HeaderOutcome {
  DISCARDED_NO_NETWORK_ERROR_LOGGING_SERVICE = 0,
  DISCARDED_INVALID_SSL_INFO = 1,
  DISCARDED_CERT_STATUS_ERROR = 2,

  DISCARDED_INSECURE_ORIGIN = 3,

  DISCARDED_JSON_TOO_BIG = 4,
  DISCARDED_JSON_INVALID = 5,
  DISCARDED_NOT_DICTIONARY = 6,
  DISCARDED_TTL_MISSING = 7,
  DISCARDED_TTL_NOT_INTEGER = 8,
  DISCARDED_TTL_NEGATIVE = 9,
  DISCARDED_REPORT_TO_MISSING = 10,
  DISCARDED_REPORT_TO_NOT_STRING = 11,

  REMOVED = 12,
  SET = 13,

  DISCARDED_MISSING_REMOTE_ENDPOINT = 14,

  MAX
};

void RecordHeaderOutcome(HeaderOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Net.NetworkErrorLogging.HeaderOutcome", outcome,
                            HeaderOutcome::MAX);
}

enum class RequestOutcome {
  DISCARDED_NO_NETWORK_ERROR_LOGGING_SERVICE = 0,

  DISCARDED_NO_REPORTING_SERVICE = 1,
  DISCARDED_INSECURE_ORIGIN = 2,
  DISCARDED_NO_ORIGIN_POLICY = 3,
  DISCARDED_UNMAPPED_ERROR = 4,
  DISCARDED_REPORTING_UPLOAD = 5,
  DISCARDED_UNSAMPLED_SUCCESS = 6,
  DISCARDED_UNSAMPLED_FAILURE = 7,
  QUEUED = 8,
  DISCARDED_NON_DNS_SUBDOMAIN_REPORT = 9,

  MAX
};

void RecordRequestOutcome(RequestOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Net.NetworkErrorLogging.RequestOutcome", outcome,
                            RequestOutcome::MAX);
}

class NetworkErrorLoggingServiceImpl : public NetworkErrorLoggingService {
 public:
  explicit NetworkErrorLoggingServiceImpl(
      std::unique_ptr<NetworkErrorLoggingDelegate> delegate)
      : delegate_(std::move(delegate)) {
    DCHECK(delegate_);
  }

  ~NetworkErrorLoggingServiceImpl() override = default;

  // NetworkErrorLoggingService implementation:

  void OnHeader(const url::Origin& origin,
                const IPAddress& received_ip_address,
                const std::string& value) override {
    // NEL is only available to secure origins, so don't permit insecure origins
    // to set policies.
    if (!origin.GetURL().SchemeIsCryptographic()) {
      RecordHeaderOutcome(HeaderOutcome::DISCARDED_INSECURE_ORIGIN);
      return;
    }

    OriginPolicy policy;
    policy.origin = origin;
    policy.received_ip_address = received_ip_address;
    HeaderOutcome outcome =
        ParseHeader(value, tick_clock_->NowTicks(), &policy);
    RecordHeaderOutcome(outcome);
    if (outcome != HeaderOutcome::SET && outcome != HeaderOutcome::REMOVED)
      return;

    auto it = policies_.find(origin);
    if (it != policies_.end()) {
      MaybeRemoveWildcardPolicy(origin, &it->second);
      policies_.erase(it);
    }

    if (policy.expires.is_null())
      return;

    auto inserted = policies_.insert(std::make_pair(origin, policy));
    DCHECK(inserted.second);
    MaybeAddWildcardPolicy(origin, &inserted.first->second);
  }

  void OnRequest(RequestDetails details) override {
    if (!reporting_service_) {
      RecordRequestOutcome(RequestOutcome::DISCARDED_NO_REPORTING_SERVICE);
      return;
    }

    // NEL is only available to secure origins, so ignore network errors from
    // insecure origins. (The check in OnHeader prevents insecure origins from
    // setting policies, but this check is needed to ensure that insecure
    // origins can't match wildcard policies from secure origins.)
    if (!details.uri.SchemeIsCryptographic()) {
      RecordRequestOutcome(RequestOutcome::DISCARDED_INSECURE_ORIGIN);
      return;
    }

    auto report_origin = url::Origin::Create(details.uri);
    const OriginPolicy* policy = FindPolicyForOrigin(report_origin);
    if (!policy) {
      RecordRequestOutcome(RequestOutcome::DISCARDED_NO_ORIGIN_POLICY);
      return;
    }

    Error type = details.type;
    // It is expected for Reporting uploads to terminate with ERR_ABORTED, since
    // the ReportingUploader cancels them after receiving the response code and
    // headers.
    if (details.reporting_upload_depth > 0 && type == ERR_ABORTED) {
      // TODO(juliatuttle): Modify ReportingUploader to drain successful uploads
      // instead of aborting them, so NEL can properly report on aborted
      // requests.
      type = OK;
    }

    std::string phase_string;
    std::string type_string;
    if (!GetPhaseAndTypeFromNetError(type, &phase_string, &type_string)) {
      RecordRequestOutcome(RequestOutcome::DISCARDED_UNMAPPED_ERROR);
      return;
    }

    if (IsHttpError(details)) {
      phase_string = kApplicationPhase;
      type_string = kHttpErrorType;
    }

    // This check would go earlier, but the histogram bucket will be more
    // meaningful if it only includes reports that otherwise could have been
    // uploaded.
    if (details.reporting_upload_depth > kMaxNestedReportDepth) {
      RecordRequestOutcome(RequestOutcome::DISCARDED_REPORTING_UPLOAD);
      return;
    }

    // If the server that handled the request is different than the server that
    // delivered the NEL policy (as determined by their IP address), then we
    // have to "downgrade" the NEL report, so that it only includes information
    // about DNS resolution.
    if (phase_string != kDnsPhase && details.server_ip.IsValid() &&
        details.server_ip != policy->received_ip_address) {
      phase_string = kDnsPhase;
      type_string = kDnsAddressChangedType;
      details.elapsed_time = base::TimeDelta();
      details.status_code = 0;
    }

    // include_subdomains policies are only allowed to report on DNS resolution
    // errors.
    if (phase_string != kDnsPhase && policy->include_subdomains &&
        !(policy->origin == report_origin)) {
      RecordRequestOutcome(RequestOutcome::DISCARDED_NON_DNS_SUBDOMAIN_REPORT);
      return;
    }

    bool success = (type == OK) && !IsHttpError(details);
    double sampling_fraction =
        success ? policy->success_fraction : policy->failure_fraction;
    if (base::RandDouble() >= sampling_fraction) {
      RecordRequestOutcome(success
                               ? RequestOutcome::DISCARDED_UNSAMPLED_SUCCESS
                               : RequestOutcome::DISCARDED_UNSAMPLED_FAILURE);
      return;
    }

    reporting_service_->QueueReport(
        details.uri, details.user_agent, policy->report_to, kReportType,
        CreateReportBody(phase_string, type_string, sampling_fraction, details),
        details.reporting_upload_depth);
    RecordRequestOutcome(RequestOutcome::QUEUED);
  }

  void RemoveBrowsingData(const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    std::vector<url::Origin> origins_to_remove;

    for (auto it = policies_.begin(); it != policies_.end(); ++it) {
      if (origin_filter.Run(it->first.GetURL()))
        origins_to_remove.push_back(it->first);
    }

    for (auto it = origins_to_remove.begin(); it != origins_to_remove.end();
         ++it) {
      MaybeRemoveWildcardPolicy(*it, &policies_[*it]);
      policies_.erase(*it);
    }
  }

  void RemoveAllBrowsingData() override {
    wildcard_policies_.clear();
    policies_.clear();
  }

  base::Value StatusAsValue() const override {
    base::Value dict(base::Value::Type::DICTIONARY);
    std::vector<base::Value> policy_list;
    // We wanted sorted (or at least reproducible) output; luckily, policies_ is
    // a std::map, and therefore already sorted.
    for (const auto& origin_and_policy : policies_) {
      const auto& origin = origin_and_policy.first;
      const auto& policy = origin_and_policy.second;
      base::Value policy_dict(base::Value::Type::DICTIONARY);
      policy_dict.SetKey("origin", base::Value(origin.Serialize()));
      policy_dict.SetKey("includeSubdomains",
                         base::Value(policy.include_subdomains));
      policy_dict.SetKey("reportTo", base::Value(policy.report_to));
      policy_dict.SetKey(
          "expires", base::Value(NetLog::TickCountToString(policy.expires)));
      policy_dict.SetKey("successFraction",
                         base::Value(policy.success_fraction));
      policy_dict.SetKey("failureFraction",
                         base::Value(policy.failure_fraction));
      policy_list.push_back(std::move(policy_dict));
    }
    dict.SetKey("originPolicies", base::Value(std::move(policy_list)));
    return dict;
  }

  std::set<url::Origin> GetPolicyOriginsForTesting() override {
    std::set<url::Origin> origins;
    for (const auto& entry : policies_) {
      origins.insert(entry.first);
    }
    return origins;
  }

 private:
  // NEL Policy set by an origin.
  struct OriginPolicy {
    url::Origin origin;
    IPAddress received_ip_address;

    // Reporting API endpoint group to which reports should be sent.
    std::string report_to;

    base::TimeTicks expires;

    double success_fraction;
    double failure_fraction;
    bool include_subdomains;
  };

  // Map from origin to origin's (owned) policy.
  // Would be unordered_map, but url::Origin has no hash.
  using PolicyMap = std::map<url::Origin, OriginPolicy>;

  // Wildcard policies are policies for which the include_subdomains flag is
  // set.
  //
  // Wildcard policies are accessed by domain name, not full origin, so there
  // can be multiple wildcard policies per domain name.
  //
  // This is a map from domain name to the set of pointers to wildcard policies
  // in that domain.
  //
  // Policies in the map are unowned; they are pointers to the original in the
  // PolicyMap.
  using WildcardPolicyMap =
      std::map<std::string, std::set<const OriginPolicy*>>;

  std::unique_ptr<NetworkErrorLoggingDelegate> delegate_;

  PolicyMap policies_;
  WildcardPolicyMap wildcard_policies_;

  HeaderOutcome ParseHeader(const std::string& json_value,
                            base::TimeTicks now_ticks,
                            OriginPolicy* policy_out) const {
    DCHECK(policy_out);

    if (json_value.size() > kMaxJsonSize)
      return HeaderOutcome::DISCARDED_JSON_TOO_BIG;

    std::unique_ptr<base::Value> value =
        base::JSONReader::Read(json_value, base::JSON_PARSE_RFC, kMaxJsonDepth);
    if (!value)
      return HeaderOutcome::DISCARDED_JSON_INVALID;

    const base::DictionaryValue* dict = nullptr;
    if (!value->GetAsDictionary(&dict))
      return HeaderOutcome::DISCARDED_NOT_DICTIONARY;

    if (!dict->HasKey(kMaxAgeKey))
      return HeaderOutcome::DISCARDED_TTL_MISSING;
    int max_age_sec;
    if (!dict->GetInteger(kMaxAgeKey, &max_age_sec))
      return HeaderOutcome::DISCARDED_TTL_NOT_INTEGER;
    if (max_age_sec < 0)
      return HeaderOutcome::DISCARDED_TTL_NEGATIVE;

    std::string report_to;
    if (max_age_sec > 0) {
      if (!dict->HasKey(kReportToKey))
        return HeaderOutcome::DISCARDED_REPORT_TO_MISSING;
      if (!dict->GetString(kReportToKey, &report_to))
        return HeaderOutcome::DISCARDED_REPORT_TO_NOT_STRING;
    }

    bool include_subdomains = false;
    // include_subdomains is optional and defaults to false, so it's okay if
    // GetBoolean fails.
    dict->GetBoolean(kIncludeSubdomainsKey, &include_subdomains);

    double success_fraction = 0.0;
    // success_fraction is optional and defaults to 0.0, so it's okay if
    // GetDouble fails.
    dict->GetDouble(kSuccessFractionKey, &success_fraction);

    double failure_fraction = 1.0;
    // failure_fraction is optional and defaults to 1.0, so it's okay if
    // GetDouble fails.
    dict->GetDouble(kFailureFractionKey, &failure_fraction);

    policy_out->report_to = report_to;
    policy_out->include_subdomains = include_subdomains;
    policy_out->success_fraction = success_fraction;
    policy_out->failure_fraction = failure_fraction;
    if (max_age_sec > 0) {
      policy_out->expires =
          now_ticks + base::TimeDelta::FromSeconds(max_age_sec);
      return HeaderOutcome::SET;
    } else {
      policy_out->expires = base::TimeTicks();
      return HeaderOutcome::REMOVED;
    }
  }

  const OriginPolicy* FindPolicyForOrigin(const url::Origin& origin) const {
    // TODO(juliatuttle): Clean out expired policies sometime/somewhere.
    auto it = policies_.find(origin);
    if (it != policies_.end() && tick_clock_->NowTicks() < it->second.expires)
      return &it->second;

    std::string domain = origin.host();
    const OriginPolicy* wildcard_policy = nullptr;
    while (!wildcard_policy && !domain.empty()) {
      wildcard_policy = FindWildcardPolicyForDomain(domain);
      domain = GetSuperdomain(domain);
    }

    return wildcard_policy;
  }

  const OriginPolicy* FindWildcardPolicyForDomain(
      const std::string& domain) const {
    DCHECK(!domain.empty());

    auto it = wildcard_policies_.find(domain);
    if (it == wildcard_policies_.end())
      return nullptr;

    DCHECK(!it->second.empty());

    // TODO(juliatuttle): Come up with a deterministic way to resolve these.
    if (it->second.size() > 1) {
      LOG(WARNING) << "Domain " << domain
                   << " matches multiple origins with include_subdomains; "
                   << "choosing one arbitrarily.";
    }

    for (auto jt = it->second.begin(); jt != it->second.end(); ++jt) {
      if (tick_clock_->NowTicks() < (*jt)->expires)
        return *jt;
    }

    return nullptr;
  }

  void MaybeAddWildcardPolicy(const url::Origin& origin,
                              const OriginPolicy* policy) {
    DCHECK(policy);
    DCHECK_EQ(policy, &policies_[origin]);

    if (!policy->include_subdomains)
      return;

    auto inserted = wildcard_policies_[origin.host()].insert(policy);
    DCHECK(inserted.second);
  }

  void MaybeRemoveWildcardPolicy(const url::Origin& origin,
                                 const OriginPolicy* policy) {
    DCHECK(policy);
    DCHECK_EQ(policy, &policies_[origin]);

    if (!policy->include_subdomains)
      return;

    auto wildcard_it = wildcard_policies_.find(origin.host());
    DCHECK(wildcard_it != wildcard_policies_.end());

    size_t erased = wildcard_it->second.erase(policy);
    DCHECK_EQ(1u, erased);
    if (wildcard_it->second.empty())
      wildcard_policies_.erase(wildcard_it);
  }

  std::unique_ptr<const base::Value> CreateReportBody(
      const std::string& phase,
      const std::string& type,
      double sampling_fraction,
      const RequestDetails& details) const {
    auto body = std::make_unique<base::DictionaryValue>();

    body->SetString(kReferrerKey, details.referrer.spec());
    body->SetDouble(kSamplingFractionKey, sampling_fraction);
    body->SetString(kServerIpKey, details.server_ip.ToString());
    body->SetString(kProtocolKey, details.protocol);
    body->SetString(kMethodKey, details.method);
    body->SetInteger(kStatusCodeKey, details.status_code);
    body->SetInteger(kElapsedTimeKey, details.elapsed_time.InMilliseconds());
    body->SetString(kPhaseKey, phase);
    body->SetString(kTypeKey, type);

    return std::move(body);
  }
};

}  // namespace

// static:

NetworkErrorLoggingService::RequestDetails::RequestDetails() = default;

NetworkErrorLoggingService::RequestDetails::RequestDetails(
    const RequestDetails& other) = default;

NetworkErrorLoggingService::RequestDetails::~RequestDetails() = default;

// static:

const char NetworkErrorLoggingService::kHeaderName[] = "NEL";

const char NetworkErrorLoggingService::kReportType[] = "network-error";

// Allow NEL reports on regular requests, plus NEL reports on Reporting uploads
// containing only regular requests, but do not allow NEL reports on Reporting
// uploads containing Reporting uploads.
//
// This prevents origins from building purposefully-broken Reporting endpoints
// that generate new NEL reports to bypass the age limit on Reporting reports.
const int NetworkErrorLoggingService::kMaxNestedReportDepth = 1;

const char NetworkErrorLoggingService::kReferrerKey[] = "referrer";
const char NetworkErrorLoggingService::kSamplingFractionKey[] =
    "sampling_fraction";
const char NetworkErrorLoggingService::kServerIpKey[] = "server_ip";
const char NetworkErrorLoggingService::kProtocolKey[] = "protocol";
const char NetworkErrorLoggingService::kMethodKey[] = "method";
const char NetworkErrorLoggingService::kStatusCodeKey[] = "status_code";
const char NetworkErrorLoggingService::kElapsedTimeKey[] = "elapsed_time";
const char NetworkErrorLoggingService::kPhaseKey[] = "phase";
const char NetworkErrorLoggingService::kTypeKey[] = "type";

// static
void NetworkErrorLoggingService::
    RecordHeaderDiscardedForNoNetworkErrorLoggingService() {
  RecordHeaderOutcome(
      HeaderOutcome::DISCARDED_NO_NETWORK_ERROR_LOGGING_SERVICE);
}

// static
void NetworkErrorLoggingService::RecordHeaderDiscardedForInvalidSSLInfo() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_INVALID_SSL_INFO);
}

// static
void NetworkErrorLoggingService::RecordHeaderDiscardedForCertStatusError() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_CERT_STATUS_ERROR);
}

// static
void NetworkErrorLoggingService::
    RecordHeaderDiscardedForMissingRemoteEndpoint() {
  RecordHeaderOutcome(HeaderOutcome::DISCARDED_MISSING_REMOTE_ENDPOINT);
}

// static
void NetworkErrorLoggingService::
    RecordRequestDiscardedForNoNetworkErrorLoggingService() {
  RecordRequestOutcome(
      RequestOutcome::DISCARDED_NO_NETWORK_ERROR_LOGGING_SERVICE);
}

// static
std::unique_ptr<NetworkErrorLoggingService> NetworkErrorLoggingService::Create(
    std::unique_ptr<NetworkErrorLoggingDelegate> delegate) {
  return std::make_unique<NetworkErrorLoggingServiceImpl>(std::move(delegate));
}

NetworkErrorLoggingService::~NetworkErrorLoggingService() = default;

void NetworkErrorLoggingService::SetReportingService(
    ReportingService* reporting_service) {
  reporting_service_ = reporting_service;
}

void NetworkErrorLoggingService::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

base::Value NetworkErrorLoggingService::StatusAsValue() const {
  NOTIMPLEMENTED();
  return base::Value();
}

std::set<url::Origin> NetworkErrorLoggingService::GetPolicyOriginsForTesting() {
  NOTIMPLEMENTED();
  return std::set<url::Origin>();
}

NetworkErrorLoggingService::NetworkErrorLoggingService()
    : tick_clock_(base::DefaultTickClock::GetInstance()),
      reporting_service_(nullptr) {}

}  // namespace net
