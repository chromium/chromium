// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_DNS_TASK_H_
#define NET_DNS_HOST_RESOLVER_DNS_TASK_H_

#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/httpssvc_metrics.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_with_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace net {

class DnsClient;
class DnsTransaction;
class DnsResponse;

// Resolves the hostname using DnsTransaction, which is a full implementation of
// a DNS stub resolver. One DnsTransaction is created for each resolution
// needed, which for AF_UNSPEC resolutions includes both A and AAAA. The
// transactions are scheduled separately and started separately.
class NET_EXPORT_PRIVATE HostResolverDnsTask final {
 public:
  using Results = std::set<std::unique_ptr<HostResolverInternalResult>>;

  // Represents a single transaction results.
  struct SingleTransactionResults {
    SingleTransactionResults(DnsQueryType query_type, Results results);
    ~SingleTransactionResults();

    SingleTransactionResults(SingleTransactionResults&&);
    SingleTransactionResults& operator=(SingleTransactionResults&&);

    SingleTransactionResults(const SingleTransactionResults&) = delete;
    SingleTransactionResults& operator=(const SingleTransactionResults&) =
        delete;

    DnsQueryType query_type;
    Results results;
  };

  class Delegate {
   public:
    virtual void OnDnsTaskComplete(base::TimeTicks start_time,
                                   bool allow_fallback,
                                   HostCache::Entry results,
                                   bool secure) = 0;

    // Called when one transaction completes successfully, or one more
    // transactions get cancelled, but only if more transactions are
    // needed. If no more transactions are needed, expect `OnDnsTaskComplete()`
    // to be called instead. `single_transaction_results` is passed only when
    // one transaction completes successfully.
    virtual void OnIntermediateTransactionsComplete(
        std::optional<SingleTransactionResults> single_transaction_results) = 0;

    virtual RequestPriority priority() const = 0;

    virtual void AddTransactionTimeQueued(base::TimeDelta time_queued) = 0;

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;
  };

  HostResolverDnsTask(DnsClient* client,
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
                      const HostResolver::HttpsSvcbOptions& https_svcb_options);
  ~HostResolverDnsTask();

  HostResolverDnsTask(const HostResolverDnsTask&) = delete;
  HostResolverDnsTask& operator=(const HostResolverDnsTask&) = delete;

  int num_additional_transactions_needed() const {
    return base::checked_cast<int>(transactions_needed_.size());
  }

  int num_transactions_in_progress() const {
    return base::checked_cast<int>(transactions_in_progress_.size());
  }

  bool secure() const { return secure_; }

  void StartNextTransaction();

  base::WeakPtr<HostResolverDnsTask> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  enum class TransactionErrorBehavior {
    // Errors lead to task fallback (immediately unless another pending/started
    // transaction has the `kFatalOrEmpty` behavior).
    kFallback,

    // Transaction errors are treated as if a NOERROR response were received,
    // allowing task success if other transactions complete successfully.
    kSynthesizeEmpty,

    // Transaction errors are potentially fatal (determined by
    // `OnTransactionComplete` and often its helper
    // `IsFatalTransactionFailure()`) for the entire Job and may disallow
    // fallback. Otherwise, same as `kSynthesizeEmpty`.
    // TODO(crbug.com/40203587): Implement the fatality behavior.
    kFatalOrEmpty,
  };

  struct TransactionInfo {
    explicit TransactionInfo(DnsQueryType type,
                             TransactionErrorBehavior error_behavior =
                                 TransactionErrorBehavior::kFallback);
    ~TransactionInfo();

    TransactionInfo(TransactionInfo&&);
    TransactionInfo& operator=(TransactionInfo&&);

    bool operator<(const TransactionInfo& other) const;

    DnsQueryType type;
    TransactionErrorBehavior error_behavior;
    std::unique_ptr<DnsTransaction> transaction;
  };

  base::Value::Dict NetLogDnsTaskCreationParams();

  base::Value::Dict NetLogDnsTaskTimeoutParams();

  DnsQueryTypeSet MaybeDisableAdditionalQueries(DnsQueryTypeSet types);

  void PushTransactionsNeeded(DnsQueryTypeSet query_types);

  void CreateAndStartTransaction(TransactionInfo transaction_info);

  void OnTimeout();

  // Called on completion of a `DnsTransaction`, but not necessarily completion
  // of all work for the individual transaction in this task (see
  // `OnTransactionsFinished()`).
  void OnDnsTransactionComplete(
      std::set<TransactionInfo>::iterator transaction_info_it,
      uint16_t request_port,
      int net_error,
      const DnsResponse* response);

  bool IsFatalTransactionFailure(int transaction_error,
                                 const TransactionInfo& transaction_info,
                                 const DnsResponse* response);

  void SortTransactionAndHandleResults(TransactionInfo transaction_info,
                                       Results transaction_results);
  void OnTransactionSorted(
      std::set<TransactionInfo>::iterator transaction_info_it,
      Results transaction_results,
      bool success,
      std::vector<IPEndPoint> sorted);
  void HandleTransactionResults(TransactionInfo transaction_info,
                                Results transaction_results);

  void OnTransactionsFinished(
      std::optional<SingleTransactionResults> single_transaction_results);

  void OnSortComplete(base::TimeTicks sort_start_time,
                      HostCache::Entry results,
                      bool secure,
                      bool success,
                      std::vector<IPEndPoint> sorted);

  bool AnyPotentiallyFatalTransactionsRemain();

  void CancelNonFatalTransactions();

  void OnFailure(
      int net_error,
      bool allow_fallback,
      std::optional<base::TimeDelta> ttl = std::nullopt,
      std::optional<DnsQueryType> failed_transaction_type = std::nullopt);

  void OnSuccess(HostCache::Entry results);

  // Returns whether any transactions left to finish are of a transaction type
  // in `types`. Used for logging and starting the timeout timer (see
  // MaybeStartTimeoutTimer()).
  bool AnyOfTypeTransactionsRemain(
      std::initializer_list<DnsQueryType> types) const;

  void MaybeStartTimeoutTimer();

  bool ShouldTriggerHttpToHttpsUpgrade(const Results& results);

  const raw_ptr<DnsClient> client_;

  HostResolver::Host host_;
  NetworkAnonymizationKey anonymization_key_;

  base::SafeRef<ResolveContext> resolve_context_;

  // Whether lookups in this DnsTask should occur using DoH or plaintext.
  const bool secure_;
  const SecureDnsMode secure_dns_mode_;

  // The listener to the results of this DnsTask.
  const raw_ptr<Delegate> delegate_;
  const NetLogWithSource net_log_;

  bool any_transaction_started_ = false;
  base::circular_deque<TransactionInfo> transactions_needed_;
  // Active transactions have iterators pointing to their entry in this set, so
  // individual entries should not be modified or removed until completion or
  // cancellation of the transaction.
  std::set<TransactionInfo> transactions_in_progress_;

  // For histograms.
  base::TimeTicks a_record_end_time_;
  base::TimeTicks aaaa_record_end_time_;

  std::optional<HostCache::Entry> saved_results_;
  bool saved_results_is_failure_ = false;

  const raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks task_start_time_;

  std::optional<HttpssvcMetrics> httpssvc_metrics_;

  // Timer for task timeout. Generally started after completion of address
  // transactions to allow aborting experimental or supplemental transactions.
  base::OneShotTimer timeout_timer_;

  // If true, there are still significant fallback options available if this
  // task completes unsuccessfully. Used as a signal that underlying
  // transactions should timeout more quickly.
  bool fallback_available_;

  const HostResolver::HttpsSvcbOptions https_svcb_options_;

  base::WeakPtrFactory<HostResolverDnsTask> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_DNS_TASK_H_
