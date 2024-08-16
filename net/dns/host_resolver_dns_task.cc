// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_dns_task.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/time/tick_clock.h"
#include "net/base/features.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_cache.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace net {

namespace {

DnsResponse CreateFakeEmptyResponse(std::string_view hostname,
                                    DnsQueryType query_type) {
  std::optional<std::vector<uint8_t>> qname =
      dns_names_util::DottedNameToNetwork(
          hostname, /*require_valid_internet_hostname=*/true);
  CHECK(qname.has_value());
  return DnsResponse::CreateEmptyNoDataResponse(
      /*id=*/0u, /*is_authoritative=*/true, qname.value(),
      DnsQueryTypeToQtype(query_type));
}

base::Value::Dict NetLogDnsTaskExtractionFailureParams(
    DnsResponseResultExtractor::ExtractionError extraction_error,
    DnsQueryType dns_query_type) {
  base::Value::Dict dict;
  dict.Set("extraction_error", base::strict_cast<int>(extraction_error));
  dict.Set("dns_query_type", kDnsQueryTypes.at(dns_query_type));
  return dict;
}

// Creates NetLog parameters when the DnsTask failed.
base::Value::Dict NetLogDnsTaskFailedParams(
    int net_error,
    std::optional<DnsQueryType> failed_transaction_type,
    std::optional<base::TimeDelta> ttl,
    const HostCache::Entry* saved_results) {
  base::Value::Dict dict;
  if (failed_transaction_type) {
    dict.Set("dns_query_type", kDnsQueryTypes.at(*failed_transaction_type));
  }
  if (ttl) {
    dict.Set("error_ttl_sec",
             base::saturated_cast<int>(ttl.value().InSeconds()));
  }
  dict.Set("net_error", net_error);
  if (saved_results) {
    dict.Set("saved_results", saved_results->NetLogParams());
  }
  return dict;
}

base::Value::Dict NetLogResults(const HostCache::Entry& results) {
  base::Value::Dict dict;
  dict.Set("results", results.NetLogParams());
  return dict;
}

void RecordResolveTimeDiffForBucket(const char* histogram_variant,
                                    const char* histogram_bucket,
                                    base::TimeDelta diff) {
  base::UmaHistogramTimes(
      base::StrCat({"Net.Dns.ResolveTimeDiff.", histogram_variant,
                    ".FirstRecord", histogram_bucket}),
      diff);
}

void RecordResolveTimeDiff(const char* histogram_variant,
                           base::TimeTicks start_time,
                           base::TimeTicks first_record_end_time,
                           base::TimeTicks second_record_end_time) {
  CHECK_LE(start_time, first_record_end_time);
  CHECK_LE(first_record_end_time, second_record_end_time);
  base::TimeDelta first_elapsed = first_record_end_time - start_time;
  base::TimeDelta diff = second_record_end_time - first_record_end_time;

  if (first_elapsed < base::Milliseconds(10)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "FasterThan10ms", diff);
  } else if (first_elapsed < base::Milliseconds(25)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "10msTo25ms", diff);
  } else if (first_elapsed < base::Milliseconds(50)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "25msTo50ms", diff);
  } else if (first_elapsed < base::Milliseconds(100)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "50msTo100ms", diff);
  } else if (first_elapsed < base::Milliseconds(250)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "100msTo250ms", diff);
  } else if (first_elapsed < base::Milliseconds(500)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "250msTo500ms", diff);
  } else if (first_elapsed < base::Seconds(1)) {
    RecordResolveTimeDiffForBucket(histogram_variant, "500msTo1s", diff);
  } else {
    RecordResolveTimeDiffForBucket(histogram_variant, "SlowerThan1s", diff);
  }
}

}  // namespace

HostResolverDnsTask::SingleTransactionResults::SingleTransactionResults(
    DnsQueryType query_type,
    Results results)
    : query_type(query_type), results(std::move(results)) {}

HostResolverDnsTask::SingleTransactionResults::~SingleTransactionResults() =
    default;

HostResolverDnsTask::SingleTransactionResults::SingleTransactionResults(
    SingleTransactionResults&&) = default;

HostResolverDnsTask::SingleTransactionResults&
HostResolverDnsTask::SingleTransactionResults::operator=(
    SingleTransactionResults&&) = default;

HostResolverDnsTask::TransactionInfo::TransactionInfo(
    DnsQueryType type,
    TransactionErrorBehavior error_behavior)
    : type(type), error_behavior(error_behavior) {}

HostResolverDnsTask::TransactionInfo::~TransactionInfo() = default;

HostResolverDnsTask::TransactionInfo::TransactionInfo(
    HostResolverDnsTask::TransactionInfo&& other) = default;

HostResolverDnsTask::TransactionInfo&
HostResolverDnsTask::TransactionInfo::operator=(
    HostResolverDnsTask::TransactionInfo&& other) = default;

bool HostResolverDnsTask::TransactionInfo::operator<(
    const HostResolverDnsTask::TransactionInfo& other) const {
  return std::tie(type, error_behavior, transaction) <
         std::tie(other.type, other.error_behavior, other.transaction);
}

HostResolverDnsTask::HostResolverDnsTask(
    DnsClient* client,
    HostResolver::Host host,
    NetworkAnonymizationKey anonymization_key,
    DnsQueryTypeSet query_types,
    ResolveContext* resolve_context,
    bool secure,
    SecureDnsMode secure_dns_mode,
    Delegate* delegate,
    const NetLogWithSource& job_net_log,
    const base::TickClock* tick_clock,
    bool fallback_available,
    const HostResolver::HttpsSvcbOptions& https_svcb_options)
    : client_(client),
      host_(std::move(host)),
      anonymization_key_(std::move(anonymization_key)),
      resolve_context_(resolve_context->AsSafeRef()),
      secure_(secure),
      secure_dns_mode_(secure_dns_mode),
      delegate_(delegate),
      net_log_(job_net_log),
      tick_clock_(tick_clock),
      task_start_time_(tick_clock_->NowTicks()),
      fallback_available_(fallback_available),
      https_svcb_options_(https_svcb_options) {
  DCHECK(client_);
  DCHECK(delegate_);

  if (!secure_) {
    DCHECK(client_->CanUseInsecureDnsTransactions());
  }

  PushTransactionsNeeded(MaybeDisableAdditionalQueries(query_types));
}

HostResolverDnsTask::~HostResolverDnsTask() = default;

void HostResolverDnsTask::StartNextTransaction() {
  DCHECK_GE(num_additional_transactions_needed(), 1);

  if (!any_transaction_started_) {
    net_log_.BeginEvent(NetLogEventType::HOST_RESOLVER_DNS_TASK,
                        [&] { return NetLogDnsTaskCreationParams(); });
  }
  any_transaction_started_ = true;

  TransactionInfo transaction_info = std::move(transactions_needed_.front());
  transactions_needed_.pop_front();

  DCHECK(IsAddressType(transaction_info.type) || secure_ ||
         client_->CanQueryAdditionalTypesViaInsecureDns());

  // Record how long this transaction has been waiting to be created.
  base::TimeDelta time_queued = tick_clock_->NowTicks() - task_start_time_;
  UMA_HISTOGRAM_LONG_TIMES_100("Net.DNS.JobQueueTime.PerTransaction",
                               time_queued);
  delegate_->AddTransactionTimeQueued(time_queued);

  CreateAndStartTransaction(std::move(transaction_info));
}

base::Value::Dict HostResolverDnsTask::NetLogDnsTaskCreationParams() {
  base::Value::Dict dict;
  dict.Set("secure", secure());

  base::Value::List transactions_needed_value;
  for (const TransactionInfo& info : transactions_needed_) {
    base::Value::Dict transaction_dict;
    transaction_dict.Set("dns_query_type", kDnsQueryTypes.at(info.type));
    transactions_needed_value.Append(std::move(transaction_dict));
  }
  dict.Set("transactions_needed", std::move(transactions_needed_value));

  return dict;
}

base::Value::Dict HostResolverDnsTask::NetLogDnsTaskTimeoutParams() {
  base::Value::Dict dict;

  if (!transactions_in_progress_.empty()) {
    base::Value::List list;
    for (const TransactionInfo& info : transactions_in_progress_) {
      base::Value::Dict transaction_dict;
      transaction_dict.Set("dns_query_type", kDnsQueryTypes.at(info.type));
      list.Append(std::move(transaction_dict));
    }
    dict.Set("started_transactions", std::move(list));
  }

  if (!transactions_needed_.empty()) {
    base::Value::List list;
    for (const TransactionInfo& info : transactions_needed_) {
      base::Value::Dict transaction_dict;
      transaction_dict.Set("dns_query_type", kDnsQueryTypes.at(info.type));
      list.Append(std::move(transaction_dict));
    }
    dict.Set("queued_transactions", std::move(list));
  }

  return dict;
}

DnsQueryTypeSet HostResolverDnsTask::MaybeDisableAdditionalQueries(
    DnsQueryTypeSet types) {
  DCHECK(!types.empty());
  DCHECK(!types.Has(DnsQueryType::UNSPECIFIED));

  // No-op if the caller explicitly requested this one query type.
  if (types.size() == 1) {
    return types;
  }

  if (types.Has(DnsQueryType::HTTPS)) {
    if (!secure_ && !client_->CanQueryAdditionalTypesViaInsecureDns()) {
      types.Remove(DnsQueryType::HTTPS);
    } else {
      DCHECK(!httpssvc_metrics_);
      httpssvc_metrics_.emplace(secure_);
    }
  }
  DCHECK(!types.empty());
  return types;
}

void HostResolverDnsTask::PushTransactionsNeeded(DnsQueryTypeSet query_types) {
  DCHECK(transactions_needed_.empty());

  if (query_types.Has(DnsQueryType::HTTPS) &&
      features::kUseDnsHttpsSvcbEnforceSecureResponse.Get() && secure_) {
    query_types.Remove(DnsQueryType::HTTPS);
    transactions_needed_.emplace_back(DnsQueryType::HTTPS,
                                      TransactionErrorBehavior::kFatalOrEmpty);
  }

  // Give AAAA/A queries a head start by pushing them to the queue first.
  constexpr DnsQueryType kHighPriorityQueries[] = {DnsQueryType::AAAA,
                                                   DnsQueryType::A};
  for (DnsQueryType high_priority_query : kHighPriorityQueries) {
    if (query_types.Has(high_priority_query)) {
      query_types.Remove(high_priority_query);
      transactions_needed_.emplace_back(high_priority_query);
    }
  }
  for (DnsQueryType remaining_query : query_types) {
    if (remaining_query == DnsQueryType::HTTPS) {
      // Ignore errors for these types. In most cases treating them normally
      // would only result in fallback to resolution without querying the
      // type. Instead, synthesize empty results.
      transactions_needed_.emplace_back(
          remaining_query, TransactionErrorBehavior::kSynthesizeEmpty);
    } else {
      transactions_needed_.emplace_back(remaining_query);
    }
  }
}

void HostResolverDnsTask::CreateAndStartTransaction(
    TransactionInfo transaction_info) {
  DCHECK(!transaction_info.transaction);
  DCHECK_NE(DnsQueryType::UNSPECIFIED, transaction_info.type);

  std::string transaction_hostname(host_.GetHostnameWithoutBrackets());

  // For HTTPS, prepend "_<port>._https." for any non-default port.
  uint16_t request_port = 0;
  if (transaction_info.type == DnsQueryType::HTTPS && host_.HasScheme()) {
    const auto& scheme_host_port = host_.AsSchemeHostPort();
    transaction_hostname =
        dns_util::GetNameForHttpsQuery(scheme_host_port, &request_port);
  }

  transaction_info.transaction =
      client_->GetTransactionFactory()->CreateTransaction(
          std::move(transaction_hostname),
          DnsQueryTypeToQtype(transaction_info.type), net_log_, secure_,
          secure_dns_mode_, &*resolve_context_,
          fallback_available_ /* fast_timeout */);
  transaction_info.transaction->SetRequestPriority(delegate_->priority());

  auto transaction_info_it =
      transactions_in_progress_.insert(std::move(transaction_info)).first;

  // Safe to pass `transaction_info_it` because it is only modified/removed
  // after async completion of this call or by destruction (which cancels the
  // transaction and prevents callback because it owns the `DnsTransaction`
  // object).
  transaction_info_it->transaction->Start(base::BindOnce(
      &HostResolverDnsTask::OnDnsTransactionComplete, base::Unretained(this),
      transaction_info_it, request_port));
}

void HostResolverDnsTask::OnTimeout() {
  net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_DNS_TASK_TIMEOUT,
                    [&] { return NetLogDnsTaskTimeoutParams(); });

  for (const TransactionInfo& transaction : transactions_in_progress_) {
    base::TimeDelta elapsed_time = tick_clock_->NowTicks() - task_start_time_;

    switch (transaction.type) {
      case DnsQueryType::HTTPS:
        DCHECK(!secure_ ||
               !features::kUseDnsHttpsSvcbEnforceSecureResponse.Get());
        if (httpssvc_metrics_) {
          // Don't record provider ID for timeouts. It is not precisely known
          // at this level which provider is actually to blame for the
          // timeout, and breaking metrics out by provider is no longer
          // important for current experimentation goals.
          httpssvc_metrics_->SaveForHttps(HttpssvcDnsRcode::kTimedOut,
                                          /*condensed_records=*/{},
                                          elapsed_time);
        }
        break;
      default:
        // The timeout timer is only started when all other transactions have
        // completed.
        NOTREACHED_IN_MIGRATION();
    }
  }

  // Clear in-progress and scheduled transactions so that
  // OnTransactionsFinished() doesn't call delegate's
  // OnIntermediateTransactionComplete().
  transactions_needed_.clear();
  transactions_in_progress_.clear();

  OnTransactionsFinished(/*single_transaction_results=*/std::nullopt);
}

void HostResolverDnsTask::OnDnsTransactionComplete(
    std::set<TransactionInfo>::iterator transaction_info_it,
    uint16_t request_port,
    int net_error,
    const DnsResponse* response) {
  CHECK(transaction_info_it != transactions_in_progress_.end(),
        base::NotFatalUntil::M130);
  DCHECK(base::Contains(transactions_in_progress_, *transaction_info_it));

  // Pull the TransactionInfo out of `transactions_in_progress_` now, so it
  // and its underlying DnsTransaction will be deleted on completion of
  // OnTransactionComplete. Note: Once control leaves OnTransactionComplete,
  // there's no further need for the transaction object. On the other hand,
  // since it owns `*response`, it should stay around while
  // OnTransactionComplete executes.
  TransactionInfo transaction_info =
      std::move(transactions_in_progress_.extract(transaction_info_it).value());

  const base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta elapsed_time = now - task_start_time_;
  enum HttpssvcDnsRcode rcode_for_httpssvc = HttpssvcDnsRcode::kNoError;
  if (httpssvc_metrics_) {
    if (net_error == ERR_DNS_TIMED_OUT) {
      rcode_for_httpssvc = HttpssvcDnsRcode::kTimedOut;
    } else if (net_error == ERR_NAME_NOT_RESOLVED) {
      rcode_for_httpssvc = HttpssvcDnsRcode::kNoError;
    } else if (response == nullptr) {
      rcode_for_httpssvc = HttpssvcDnsRcode::kMissingDnsResponse;
    } else {
      rcode_for_httpssvc =
          TranslateDnsRcodeForHttpssvcExperiment(response->rcode());
    }
  }

  // Handle network errors. Note that for NXDOMAIN, DnsTransaction returns
  // ERR_NAME_NOT_RESOLVED, so that is not a network error if received with a
  // valid response.
  bool fatal_error =
      IsFatalTransactionFailure(net_error, transaction_info, response);
  std::optional<DnsResponse> fake_response;
  if (net_error != OK && !(net_error == ERR_NAME_NOT_RESOLVED && response &&
                           response->IsValid())) {
    if (transaction_info.error_behavior ==
            TransactionErrorBehavior::kFallback ||
        fatal_error) {
      // Fail task (or maybe Job) completely on network failure.
      OnFailure(net_error, /*allow_fallback=*/!fatal_error,
                /*ttl=*/std::nullopt, transaction_info.type);
      return;
    } else {
      DCHECK((transaction_info.error_behavior ==
                  TransactionErrorBehavior::kFatalOrEmpty &&
              !fatal_error) ||
             transaction_info.error_behavior ==
                 TransactionErrorBehavior::kSynthesizeEmpty);
      // For non-fatal failures, synthesize an empty response.
      fake_response = CreateFakeEmptyResponse(
          host_.GetHostnameWithoutBrackets(), transaction_info.type);
      response = &fake_response.value();
    }
  }

  DCHECK(response);

  DnsResponseResultExtractor::ResultsOrError results;
  {
    // Scope the extractor to ensure it is destroyed before `response`.
    DnsResponseResultExtractor extractor(*response);
    results = extractor.ExtractDnsResults(
        transaction_info.type,
        /*original_domain_name=*/host_.GetHostnameWithoutBrackets(),
        request_port);
  }

  DCHECK_NE(results.error_or(DnsResponseResultExtractor::ExtractionError::kOk),
            DnsResponseResultExtractor::ExtractionError::kUnexpected);

  if (!results.has_value()) {
    net_log_.AddEvent(
        NetLogEventType::HOST_RESOLVER_DNS_TASK_EXTRACTION_FAILURE, [&] {
          return NetLogDnsTaskExtractionFailureParams(results.error(),
                                                      transaction_info.type);
        });
    if (transaction_info.error_behavior ==
            TransactionErrorBehavior::kFatalOrEmpty ||
        transaction_info.error_behavior ==
            TransactionErrorBehavior::kSynthesizeEmpty) {
      // No extraction errors are currently considered fatal, otherwise, there
      // would need to be a call to some sort of
      // IsFatalTransactionExtractionError() function.
      DCHECK(!fatal_error);
      DCHECK_EQ(transaction_info.type, DnsQueryType::HTTPS);
      results = Results();
    } else {
      OnFailure(ERR_DNS_MALFORMED_RESPONSE, /*allow_fallback=*/true,
                /*ttl=*/std::nullopt, transaction_info.type);
      return;
    }
  }
  CHECK(results.has_value());
  net_log_.AddEvent(NetLogEventType::HOST_RESOLVER_DNS_TASK_EXTRACTION_RESULTS,
                    [&] {
                      base::Value::List list;
                      list.reserve(results.value().size());
                      for (const auto& result : results.value()) {
                        list.Append(result->ToValue());
                      }
                      base::Value::Dict dict;
                      dict.Set("results", std::move(list));
                      return dict;
                    });

  if (httpssvc_metrics_) {
    if (transaction_info.type == DnsQueryType::HTTPS) {
      bool has_compatible_https = base::ranges::any_of(
          results.value(),
          [](const std::unique_ptr<HostResolverInternalResult>& result) {
            return result->type() ==
                   HostResolverInternalResult::Type::kMetadata;
          });
      if (has_compatible_https) {
        httpssvc_metrics_->SaveForHttps(rcode_for_httpssvc,
                                        std::vector<bool>{true}, elapsed_time);
      } else {
        httpssvc_metrics_->SaveForHttps(rcode_for_httpssvc, std::vector<bool>(),
                                        elapsed_time);
      }
    } else {
      httpssvc_metrics_->SaveForAddressQuery(elapsed_time, rcode_for_httpssvc);
    }
  }

  switch (transaction_info.type) {
    case DnsQueryType::A:
      a_record_end_time_ = now;
      if (!aaaa_record_end_time_.is_null()) {
        RecordResolveTimeDiff("AAAABeforeA", task_start_time_,
                              aaaa_record_end_time_, a_record_end_time_);
      }
      break;
    case DnsQueryType::AAAA:
      aaaa_record_end_time_ = now;
      if (!a_record_end_time_.is_null()) {
        RecordResolveTimeDiff("ABeforeAAAA", task_start_time_,
                              a_record_end_time_, aaaa_record_end_time_);
      }
      break;
    case DnsQueryType::HTTPS: {
      base::TimeTicks first_address_end_time =
          std::min(a_record_end_time_, aaaa_record_end_time_);
      if (!first_address_end_time.is_null()) {
        RecordResolveTimeDiff("AddressRecordBeforeHTTPS", task_start_time_,
                              first_address_end_time, now);
      }
      break;
    }
    default:
      break;
  }

  if (base::FeatureList::IsEnabled(features::kUseHostResolverCache) ||
      base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    SortTransactionAndHandleResults(std::move(transaction_info),
                                    std::move(results).value());
  } else {
    HandleTransactionResults(std::move(transaction_info),
                             std::move(results).value());
  }
}

bool HostResolverDnsTask::IsFatalTransactionFailure(
    int transaction_error,
    const TransactionInfo& transaction_info,
    const DnsResponse* response) {
  if (transaction_info.type != DnsQueryType::HTTPS) {
    DCHECK(transaction_info.error_behavior !=
           TransactionErrorBehavior::kFatalOrEmpty);
    return false;
  }

  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "DNS.SvcbHttpsTransactionError" in
  // src/tools/metrics/histograms/enums.xml.
  enum class HttpsTransactionError {
    kNoError = 0,
    kInsecureError = 1,
    kNonFatalError = 2,
    kFatalErrorDisabled = 3,
    kFatalErrorEnabled = 4,
    kMaxValue = kFatalErrorEnabled
  } error;

  if (transaction_error == OK || (transaction_error == ERR_NAME_NOT_RESOLVED &&
                                  response && response->IsValid())) {
    error = HttpsTransactionError::kNoError;
  } else if (!secure_) {
    // HTTPS failures are never fatal via insecure DNS.
    DCHECK(transaction_info.error_behavior !=
           TransactionErrorBehavior::kFatalOrEmpty);
    error = HttpsTransactionError::kInsecureError;
  } else if (transaction_error == ERR_DNS_SERVER_FAILED && response &&
             response->rcode() != dns_protocol::kRcodeSERVFAIL) {
    // For server failures, only SERVFAIL is fatal.
    error = HttpsTransactionError::kNonFatalError;
  } else if (features::kUseDnsHttpsSvcbEnforceSecureResponse.Get()) {
    DCHECK(transaction_info.error_behavior ==
           TransactionErrorBehavior::kFatalOrEmpty);
    error = HttpsTransactionError::kFatalErrorEnabled;
  } else {
    DCHECK(transaction_info.error_behavior !=
           TransactionErrorBehavior::kFatalOrEmpty);
    error = HttpsTransactionError::kFatalErrorDisabled;
  }

  UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTask.SvcbHttpsTransactionError", error);
  return error == HttpsTransactionError::kFatalErrorEnabled;
}

void HostResolverDnsTask::SortTransactionAndHandleResults(
    TransactionInfo transaction_info,
    Results transaction_results) {
  // Expect at most 1 data result in an individual transaction.
  CHECK_LE(base::ranges::count_if(
               transaction_results,
               [](const std::unique_ptr<HostResolverInternalResult>& result) {
                 return result->type() ==
                        HostResolverInternalResult::Type::kData;
               }),
           1);

  auto data_result_it = base::ranges::find_if(
      transaction_results,
      [](const std::unique_ptr<HostResolverInternalResult>& result) {
        return result->type() == HostResolverInternalResult::Type::kData;
      });

  std::vector<IPEndPoint> endpoints_to_sort;
  if (data_result_it != transaction_results.end()) {
    const HostResolverInternalDataResult& data_result =
        (*data_result_it)->AsData();
    endpoints_to_sort.insert(endpoints_to_sort.end(),
                             data_result.endpoints().begin(),
                             data_result.endpoints().end());
  }

  if (!endpoints_to_sort.empty()) {
    // More async work to do, so insert `transaction_info` back onto
    // `transactions_in_progress_`.
    auto insertion_result =
        transactions_in_progress_.insert(std::move(transaction_info));
    CHECK(insertion_result.second);

    // Sort() potentially calls OnTransactionSorted() synchronously.
    client_->GetAddressSorter()->Sort(
        endpoints_to_sort,
        base::BindOnce(&HostResolverDnsTask::OnTransactionSorted,
                       weak_ptr_factory_.GetWeakPtr(), insertion_result.first,
                       std::move(transaction_results)));
  } else {
    HandleTransactionResults(std::move(transaction_info),
                             std::move(transaction_results));
  }
}

void HostResolverDnsTask::OnTransactionSorted(
    std::set<TransactionInfo>::iterator transaction_info_it,
    Results transaction_results,
    bool success,
    std::vector<IPEndPoint> sorted) {
  CHECK(transaction_info_it != transactions_in_progress_.end());

  if (transactions_in_progress_.find(*transaction_info_it) ==
      transactions_in_progress_.end()) {
    // If no longer in `transactions_in_progress_`, transaction was cancelled.
    // Do nothing.
    return;
  }
  TransactionInfo transaction_info =
      std::move(transactions_in_progress_.extract(transaction_info_it).value());

  // Expect exactly one data result.
  auto data_result_it = base::ranges::find_if(
      transaction_results,
      [](const std::unique_ptr<HostResolverInternalResult>& result) {
        return result->type() == HostResolverInternalResult::Type::kData;
      });
  CHECK(data_result_it != transaction_results.end());
  DCHECK_EQ(base::ranges::count_if(
                transaction_results,
                [](const std::unique_ptr<HostResolverInternalResult>& result) {
                  return result->type() ==
                         HostResolverInternalResult::Type::kData;
                }),
            1);

  if (!success) {
    // If sort failed, replace data result with a TTL-containing error result.
    auto error_replacement = std::make_unique<HostResolverInternalErrorResult>(
        (*data_result_it)->domain_name(), (*data_result_it)->query_type(),
        (*data_result_it)->expiration(), (*data_result_it)->timed_expiration(),
        HostResolverInternalResult::Source::kUnknown, ERR_DNS_SORT_ERROR);
    CHECK(error_replacement->expiration().has_value());
    CHECK(error_replacement->timed_expiration().has_value());

    transaction_results.erase(data_result_it);
    transaction_results.insert(std::move(error_replacement));
  } else if (sorted.empty()) {
    // Sorter prunes unusable destinations. If all addresses are pruned,
    // remove the data result and replace with TTL-containing error result.
    auto error_replacement = std::make_unique<HostResolverInternalErrorResult>(
        (*data_result_it)->domain_name(), (*data_result_it)->query_type(),
        (*data_result_it)->expiration(), (*data_result_it)->timed_expiration(),
        (*data_result_it)->source(), ERR_NAME_NOT_RESOLVED);
    CHECK(error_replacement->expiration().has_value());
    CHECK(error_replacement->timed_expiration().has_value());

    transaction_results.erase(data_result_it);
    transaction_results.insert(std::move(error_replacement));
  } else {
    (*data_result_it)->AsData().set_endpoints(std::move(sorted));
  }

  HandleTransactionResults(std::move(transaction_info),
                           std::move(transaction_results));
}

void HostResolverDnsTask::HandleTransactionResults(
    TransactionInfo transaction_info,
    Results transaction_results) {
  CHECK(transactions_in_progress_.find(transaction_info) ==
        transactions_in_progress_.end());

  if (base::FeatureList::IsEnabled(features::kUseHostResolverCache) &&
      resolve_context_->host_resolver_cache() != nullptr) {
    for (const std::unique_ptr<HostResolverInternalResult>& result :
         transaction_results) {
      resolve_context_->host_resolver_cache()->Set(
          result->Clone(), anonymization_key_, HostResolverSource::DNS,
          secure_);
    }
  }

  // Trigger HTTP->HTTPS upgrade if an HTTPS record is received for an "http"
  // or "ws" request.
  if (transaction_info.type == DnsQueryType::HTTPS &&
      ShouldTriggerHttpToHttpsUpgrade(transaction_results)) {
    // Disallow fallback. Otherwise DNS could be reattempted without HTTPS
    // queries, and that would hide this error instead of triggering upgrade.
    OnFailure(
        ERR_DNS_NAME_HTTPS_ONLY, /*allow_fallback=*/false,
        HostCache::Entry::TtlFromInternalResults(
            transaction_results, base::Time::Now(), tick_clock_->NowTicks()),
        transaction_info.type);
    return;
  }

  // Failures other than ERR_NAME_NOT_RESOLVED cannot be merged with other
  // transactions.
  auto failure_result_it = base::ranges::find_if(
      transaction_results,
      [](const std::unique_ptr<HostResolverInternalResult>& result) {
        return result->type() == HostResolverInternalResult::Type::kError;
      });
  DCHECK_LE(base::ranges::count_if(
                transaction_results,
                [](const std::unique_ptr<HostResolverInternalResult>& result) {
                  return result->type() ==
                         HostResolverInternalResult::Type::kError;
                }),
            1);
  if (failure_result_it != transaction_results.end() &&
      (*failure_result_it)->AsError().error() != ERR_NAME_NOT_RESOLVED) {
    OnFailure(
        (*failure_result_it)->AsError().error(), /*allow_fallback=*/true,
        HostCache::Entry::TtlFromInternalResults(
            transaction_results, base::Time::Now(), tick_clock_->NowTicks()),
        transaction_info.type);
    return;
  }

  // TODO(crbug.com/40245250): Use new results type directly instead of
  // converting to HostCache::Entry.
  HostCache::Entry legacy_results(transaction_results, base::Time::Now(),
                                  tick_clock_->NowTicks(),
                                  HostCache::Entry::SOURCE_DNS);

  // Merge results with saved results from previous transactions.
  if (saved_results_) {
    // If saved result is a deferred failure, try again to complete with that
    // failure.
    if (saved_results_is_failure_) {
      OnFailure(saved_results_.value().error(), /*allow_fallback=*/true,
                saved_results_.value().GetOptionalTtl());
      return;
    }

    switch (transaction_info.type) {
      case DnsQueryType::A:
        // Canonical names from A results have lower priority than those
        // from AAAA results, so merge to the back.
        legacy_results = HostCache::Entry::MergeEntries(
            std::move(saved_results_).value(), std::move(legacy_results));
        break;
      case DnsQueryType::AAAA:
        // Canonical names from AAAA results take priority over those
        // from A results, so merge to the front.
        legacy_results = HostCache::Entry::MergeEntries(
            std::move(legacy_results), std::move(saved_results_).value());
        break;
      case DnsQueryType::HTTPS:
        // No particular importance to order.
        legacy_results = HostCache::Entry::MergeEntries(
            std::move(legacy_results), std::move(saved_results_).value());
        break;
      default:
        // Only expect address query types with multiple transactions.
        NOTREACHED_IN_MIGRATION();
    }
  }

  saved_results_ = std::move(legacy_results);

  OnTransactionsFinished(SingleTransactionResults(
      transaction_info.type, std::move(transaction_results)));
}

void HostResolverDnsTask::OnTransactionsFinished(
    std::optional<SingleTransactionResults> single_transaction_results) {
  if (!transactions_in_progress_.empty() || !transactions_needed_.empty()) {
    MaybeStartTimeoutTimer();
    delegate_->OnIntermediateTransactionsComplete(
        std::move(single_transaction_results));
    // `this` may be deleted by `delegate_`. Do not add code below.
    return;
  }

  DCHECK(saved_results_.has_value());
  HostCache::Entry results = std::move(*saved_results_);

  timeout_timer_.Stop();

  // If using HostResolverCache, transactions are already invidvidually sorted
  // on completion.
  if (!base::FeatureList::IsEnabled(features::kUseHostResolverCache)) {
    std::vector<IPEndPoint> ip_endpoints = results.ip_endpoints();

    // If there are multiple addresses, and at least one is IPv6, need to
    // sort them.
    bool at_least_one_ipv6_address = base::ranges::any_of(
        ip_endpoints,
        [](auto& e) { return e.GetFamily() == ADDRESS_FAMILY_IPV6; });

    if (at_least_one_ipv6_address) {
      // Sort addresses if needed.  Sort could complete synchronously.
      client_->GetAddressSorter()->Sort(
          ip_endpoints,
          base::BindOnce(&HostResolverDnsTask::OnSortComplete,
                         weak_ptr_factory_.GetWeakPtr(),
                         tick_clock_->NowTicks(), std::move(results), secure_));
      return;
    }
  }

  OnSuccess(std::move(results));
}

void HostResolverDnsTask::OnSortComplete(base::TimeTicks sort_start_time,
                                         HostCache::Entry results,
                                         bool secure,
                                         bool success,
                                         std::vector<IPEndPoint> sorted) {
  results.set_ip_endpoints(std::move(sorted));

  if (!success) {
    OnFailure(ERR_DNS_SORT_ERROR, /*allow_fallback=*/true,
              results.GetOptionalTtl());
    return;
  }

  // AddressSorter prunes unusable destinations.
  if (results.ip_endpoints().empty() && results.text_records().empty() &&
      results.hostnames().empty()) {
    LOG(WARNING) << "Address list empty after RFC3484 sort";
    OnFailure(ERR_NAME_NOT_RESOLVED, /*allow_fallback=*/true,
              results.GetOptionalTtl());
    return;
  }

  OnSuccess(std::move(results));
}

bool HostResolverDnsTask::AnyPotentiallyFatalTransactionsRemain() {
  auto is_fatal_or_empty_error = [](TransactionErrorBehavior behavior) {
    return behavior == TransactionErrorBehavior::kFatalOrEmpty;
  };

  return base::ranges::any_of(transactions_needed_, is_fatal_or_empty_error,
                              &TransactionInfo::error_behavior) ||
         base::ranges::any_of(transactions_in_progress_,
                              is_fatal_or_empty_error,
                              &TransactionInfo::error_behavior);
}

void HostResolverDnsTask::CancelNonFatalTransactions() {
  auto has_non_fatal_or_empty_error = [](const TransactionInfo& info) {
    return info.error_behavior != TransactionErrorBehavior::kFatalOrEmpty;
  };

  base::EraseIf(transactions_needed_, has_non_fatal_or_empty_error);
  std::erase_if(transactions_in_progress_, has_non_fatal_or_empty_error);
}

void HostResolverDnsTask::OnFailure(
    int net_error,
    bool allow_fallback,
    std::optional<base::TimeDelta> ttl,
    std::optional<DnsQueryType> failed_transaction_type) {
  if (httpssvc_metrics_ && failed_transaction_type.has_value() &&
      IsAddressType(failed_transaction_type.value())) {
    httpssvc_metrics_->SaveAddressQueryFailure();
  }

  DCHECK_NE(OK, net_error);
  HostCache::Entry results(net_error, HostCache::Entry::SOURCE_UNKNOWN, ttl);

  // On non-fatal errors, if any potentially fatal transactions remain, need
  // to defer ending the task in case any of those remaining transactions end
  // with a fatal failure.
  if (allow_fallback && AnyPotentiallyFatalTransactionsRemain()) {
    saved_results_ = std::move(results);
    saved_results_is_failure_ = true;

    CancelNonFatalTransactions();
    OnTransactionsFinished(/*single_transaction_results=*/std::nullopt);
    return;
  }

  net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_DNS_TASK, [&] {
    return NetLogDnsTaskFailedParams(net_error, failed_transaction_type, ttl,
                                     base::OptionalToPtr(saved_results_));
  });

  // Expect this to result in destroying `this` and thus cancelling any
  // remaining transactions.
  delegate_->OnDnsTaskComplete(task_start_time_, allow_fallback,
                               std::move(results), secure_);
}

void HostResolverDnsTask::OnSuccess(HostCache::Entry results) {
  net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_DNS_TASK,
                    [&] { return NetLogResults(results); });
  delegate_->OnDnsTaskComplete(task_start_time_, /*allow_fallback=*/true,
                               std::move(results), secure_);
}

bool HostResolverDnsTask::AnyOfTypeTransactionsRemain(
    std::initializer_list<DnsQueryType> types) const {
  // Should only be called if some transactions are still running or waiting
  // to run.
  DCHECK(!transactions_needed_.empty() || !transactions_in_progress_.empty());

  // Check running transactions.
  if (base::ranges::find_first_of(transactions_in_progress_, types,
                                  /*pred=*/{},
                                  /*proj1=*/&TransactionInfo::type) !=
      transactions_in_progress_.end()) {
    return true;
  }

  // Check queued transactions, in case it ever becomes possible to get here
  // without the transactions being started first.
  return base::ranges::find_first_of(transactions_needed_, types, /*pred=*/{},
                                     /*proj1=*/&TransactionInfo::type) !=
         transactions_needed_.end();
}

void HostResolverDnsTask::MaybeStartTimeoutTimer() {
  // Should only be called if some transactions are still running or waiting
  // to run.
  DCHECK(!transactions_in_progress_.empty() || !transactions_needed_.empty());

  // Timer already running.
  if (timeout_timer_.IsRunning()) {
    return;
  }

  // Always wait for address transactions.
  if (AnyOfTypeTransactionsRemain({DnsQueryType::A, DnsQueryType::AAAA})) {
    return;
  }

  base::TimeDelta timeout_max;
  int extra_time_percent = 0;
  base::TimeDelta timeout_min;

  if (AnyOfTypeTransactionsRemain({DnsQueryType::HTTPS})) {
    DCHECK(https_svcb_options_.enable);

    if (secure_) {
      timeout_max = https_svcb_options_.secure_extra_time_max;
      extra_time_percent = https_svcb_options_.secure_extra_time_percent;
      timeout_min = https_svcb_options_.secure_extra_time_min;
    } else {
      timeout_max = https_svcb_options_.insecure_extra_time_max;
      extra_time_percent = https_svcb_options_.insecure_extra_time_percent;
      timeout_min = https_svcb_options_.insecure_extra_time_min;
    }

    // Skip timeout for secure requests if the timeout would be a fatal
    // failure.
    if (secure_ && features::kUseDnsHttpsSvcbEnforceSecureResponse.Get()) {
      timeout_max = base::TimeDelta();
      extra_time_percent = 0;
      timeout_min = base::TimeDelta();
    }
  } else {
    // Unhandled supplemental type.
    NOTREACHED_IN_MIGRATION();
  }

  base::TimeDelta timeout;
  if (extra_time_percent > 0) {
    base::TimeDelta total_time_for_other_transactions =
        tick_clock_->NowTicks() - task_start_time_;
    timeout = total_time_for_other_transactions * extra_time_percent / 100;
    // Use at least 1ms to ensure timeout doesn't occur immediately in tests.
    timeout = std::max(timeout, base::Milliseconds(1));

    if (!timeout_max.is_zero()) {
      timeout = std::min(timeout, timeout_max);
    }
    if (!timeout_min.is_zero()) {
      timeout = std::max(timeout, timeout_min);
    }
  } else {
    // If no relative timeout, use a non-zero min/max as timeout. If both are
    // non-zero, that's not very sensible, but arbitrarily take the higher
    // timeout.
    timeout = std::max(timeout_min, timeout_max);
  }

  if (!timeout.is_zero()) {
    timeout_timer_.Start(FROM_HERE, timeout,
                         base::BindOnce(&HostResolverDnsTask::OnTimeout,
                                        base::Unretained(this)));
  }
}

bool HostResolverDnsTask::ShouldTriggerHttpToHttpsUpgrade(
    const Results& results) {
  // Upgrade if at least one HTTPS record was compatible, and the host uses an
  // upgradable scheme.

  if (!host_.HasScheme()) {
    return false;
  }

  const std::string& scheme = host_.GetScheme();
  if (scheme != url::kHttpScheme && scheme != url::kWsScheme) {
    return false;
  }

  return base::ranges::any_of(
      results, [](const std::unique_ptr<HostResolverInternalResult>& result) {
        return result->type() == HostResolverInternalResult::Type::kMetadata;
      });
}

}  // namespace net
