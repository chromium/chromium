// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_mdns_task.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"

namespace net {

namespace {
std::unique_ptr<HostResolverInternalResult> ParseHostnameResult(
    std::string query_name,
    DnsQueryType query_type,
    std::string_view host,
    uint16_t port) {
  // Filter out root domain. Depending on the type, it either means no-result
  // or is simply not a result important to any expected Chrome usecases.
  if (host.empty()) {
    return std::make_unique<HostResolverInternalErrorResult>(
        std::move(query_name), query_type,
        /*expiration=*/std::nullopt,
        /*timed_expiration=*/std::nullopt,
        HostResolverInternalResult::Source::kUnknown, ERR_NAME_NOT_RESOLVED);
  } else {
    return std::make_unique<HostResolverInternalDataResult>(
        std::move(query_name), query_type,
        /*expiration=*/base::TimeTicks::Now(),
        /*timed_expiration=*/base::Time::Now(),
        HostResolverInternalResult::Source::kUnknown,
        /*endpoints=*/std::vector<IPEndPoint>{},
        /*strings=*/std::vector<std::string>{},
        /*hosts=*/
        std::vector<HostPortPair>{HostPortPair(host, port)});
  }
}
}  // namespace

class HostResolverMdnsTask::Transaction {
 public:
  Transaction(DnsQueryType query_type, HostResolverMdnsTask* task)
      : query_type_(query_type),
        task_(task) {}

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);

    // Should not be completed or running yet.
    DCHECK(!result_);
    DCHECK(!async_transaction_);

    // TODO(crbug.com/40611558): Use |allow_cached_response| to set the
    // QUERY_CACHE flag or not.
    int flags = MDnsTransaction::SINGLE_RESULT | MDnsTransaction::QUERY_CACHE |
                MDnsTransaction::QUERY_NETWORK;
    // If |this| is destroyed, destruction of |internal_transaction_| should
    // cancel and prevent invocation of OnComplete.
    std::unique_ptr<MDnsTransaction> inner_transaction =
        task_->mdns_client_->CreateTransaction(
            DnsQueryTypeToQtype(query_type_), task_->hostname_, flags,
            base::BindRepeating(&HostResolverMdnsTask::Transaction::OnComplete,
                                base::Unretained(this)));

    // Side effect warning: Start() may finish and invoke callbacks inline.
    bool start_result = inner_transaction->Start();

    if (!start_result) {
      task_->Complete(true /* post_needed */);
    } else if (!result_) {
      async_transaction_ = std::move(inner_transaction);
    }
  }

  bool IsDone() const { return !!result_; }
  bool IsError() const {
    return IsDone() &&
           result_->type() == HostResolverInternalResult::Type::kError &&
           result_->AsError().error() != ERR_NAME_NOT_RESOLVED;
  }
  const HostResolverInternalResult* result() const { return result_.get(); }

  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);
    DCHECK(!result_);

    result_ = std::make_unique<HostResolverInternalErrorResult>(
        task_->hostname_, query_type_, /*expiration=*/std::nullopt,
        /*timed_expiration=*/std::nullopt,
        HostResolverInternalResult::Source::kUnknown, ERR_FAILED);
  }

 private:
  void OnComplete(MDnsTransaction::Result result, const RecordParsed* parsed) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);
    DCHECK(!result_);

    int error = ERR_UNEXPECTED;
    switch (result) {
      case MDnsTransaction::RESULT_RECORD:
        DCHECK(parsed);
        error = OK;
        break;
      case MDnsTransaction::RESULT_NO_RESULTS:
      case MDnsTransaction::RESULT_NSEC:
        error = ERR_NAME_NOT_RESOLVED;
        break;
      default:
        // No other results should be possible with the request flags used.
        NOTREACHED();
    }

    result_ = HostResolverMdnsTask::ParseResult(error, task_->hostname_,
                                                query_type_, parsed);

    // If we don't have a saved async_transaction, it means OnComplete was
    // invoked inline in MDnsTransaction::Start. Callbacks will need to be
    // invoked via post.
    task_->CheckCompletion(!async_transaction_);
  }

  const DnsQueryType query_type_;

  // null until transaction completes (or is cancelled).
  std::unique_ptr<HostResolverInternalResult> result_;

  // Not saved until MDnsTransaction::Start completes to differentiate inline
  // completion.
  std::unique_ptr<MDnsTransaction> async_transaction_;

  // Back pointer. Expected to destroy |this| before destroying itself.
  const raw_ptr<HostResolverMdnsTask> task_;
};

HostResolverMdnsTask::HostResolverMdnsTask(MDnsClient* mdns_client,
                                           std::string hostname,
                                           DnsQueryTypeSet query_types)
    : mdns_client_(mdns_client), hostname_(std::move(hostname)) {
  CHECK(!query_types.empty());
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));

  static constexpr DnsQueryTypeSet kUnwantedQueries = {DnsQueryType::HTTPS};

  for (DnsQueryType query_type : Difference(query_types, kUnwantedQueries)) {
    transactions_.emplace_back(query_type, this);
  }
  CHECK(!transactions_.empty()) << "Only unwanted query types supplied.";
}

HostResolverMdnsTask::~HostResolverMdnsTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transactions_.clear();
}

void HostResolverMdnsTask::Start(base::OnceClosure completion_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!completion_closure_);
  DCHECK(mdns_client_);

  completion_closure_ = std::move(completion_closure);

  for (auto& transaction : transactions_) {
    // Only start transaction if it is not already marked done. A transaction
    // could be marked done before starting if it is preemptively canceled by
    // a previously started transaction finishing with an error.
    if (!transaction.IsDone())
      transaction.Start();
  }
}

// TODO(ericorth@chromium.org): This is a bit wasteful in always copying out
// results from `transactions_`. We should consider moving out the results,
// either by making it a requirement to only call GetResults() once or by
// refactoring to pass the results out once with the completion signal.
std::set<std::unique_ptr<HostResolverInternalResult>>
HostResolverMdnsTask::GetResults() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!transactions_.empty());
  DCHECK(!completion_closure_);
  DCHECK(std::ranges::all_of(transactions_,
                             [](const Transaction& t) { return t.IsDone(); }));

  std::set<std::unique_ptr<HostResolverInternalResult>> combined_results;

  auto found_error = std::ranges::find_if(transactions_, &Transaction::IsError);
  if (found_error == transactions_.end()) {
    for (const Transaction& transaction : transactions_) {
      CHECK(transaction.result());
      combined_results.insert(transaction.result()->Clone());
    }
  } else {
    CHECK(found_error->result());
    combined_results.insert(found_error->result()->Clone());
  }

  return combined_results;
}

// static
//
// Because MDnsClient does its own internal caching, true expiration times are
// not known here and cannot be determined from the TTL in `parsed`.
// Fortunately, HostResolverManager is not expected to try to cache mDNS results
// (partly because it assumes the existence of that internal cache), so it is
// not necessary to fill useful information into the expiration fields of the
// results returned here. For the sake of filling something in, use
// `base::TimeTicks::Now()` as the expiration in data results.
std::unique_ptr<HostResolverInternalResult> HostResolverMdnsTask::ParseResult(
    int error,
    std::string query_hostname,
    DnsQueryType query_type,
    const RecordParsed* parsed) {
  if (error != OK) {
    return std::make_unique<HostResolverInternalErrorResult>(
        std::move(query_hostname), query_type,
        /*expiration=*/std::nullopt,
        /*timed_expiration=*/std::nullopt,
        HostResolverInternalResult::Source::kUnknown, error);
  }
  DCHECK(parsed);

  // Expected to be validated by MDnsClient.
  DCHECK_EQ(DnsQueryTypeToQtype(query_type), parsed->type());
  DCHECK(base::EqualsCaseInsensitiveASCII(query_hostname, parsed->name()));

  switch (query_type) {
    case DnsQueryType::UNSPECIFIED:
      // Should create two separate transactions with specified type.
    case DnsQueryType::HTTPS:
      // Not supported.
      // TODO(ericorth@chromium.org): Consider support for HTTPS in mDNS if it
      // is ever decided to support HTTPS via non-DoH.
      NOTREACHED();
    case DnsQueryType::A:
      return std::make_unique<HostResolverInternalDataResult>(
          std::move(query_hostname), query_type,
          /*expiration=*/base::TimeTicks::Now(),
          /*timed_expiration=*/base::Time::Now(),
          HostResolverInternalResult::Source::kUnknown,
          std::vector<IPEndPoint>{
              IPEndPoint(parsed->rdata<net::ARecordRdata>()->address(), 0)},
          /*strings=*/std::vector<std::string>{},
          /*hosts=*/std::vector<HostPortPair>{});
    case DnsQueryType::AAAA:
      return std::make_unique<HostResolverInternalDataResult>(
          std::move(query_hostname), query_type,
          /*expiration=*/base::TimeTicks::Now(),
          /*timed_expiration=*/base::Time::Now(),
          HostResolverInternalResult::Source::kUnknown,
          std::vector<IPEndPoint>{
              IPEndPoint(parsed->rdata<net::AAAARecordRdata>()->address(), 0)},
          /*strings=*/std::vector<std::string>{},
          /*hosts=*/std::vector<HostPortPair>{});
    case DnsQueryType::TXT:
      // TXT invalid without at least one string. If none, should be rejected by
      // parser.
      CHECK(!parsed->rdata<net::TxtRecordRdata>()->texts().empty());
      return std::make_unique<HostResolverInternalDataResult>(
          std::move(query_hostname), query_type,
          /*expiration=*/base::TimeTicks::Now(),
          /*timed_expiration=*/base::Time::Now(),
          HostResolverInternalResult::Source::kUnknown,
          /*endpoints=*/std::vector<IPEndPoint>{},
          /*strings=*/parsed->rdata<net::TxtRecordRdata>()->texts(),
          /*hosts=*/std::vector<HostPortPair>{});
    case DnsQueryType::PTR:
      return ParseHostnameResult(std::move(query_hostname), query_type,
                                 parsed->rdata<PtrRecordRdata>()->ptrdomain(),
                                 /*port=*/0);
    case DnsQueryType::SRV:
      return ParseHostnameResult(std::move(query_hostname), query_type,
                                 parsed->rdata<SrvRecordRdata>()->target(),
                                 parsed->rdata<SrvRecordRdata>()->port());
  }
}

void HostResolverMdnsTask::CheckCompletion(bool post_needed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Finish immediately if any transactions completed with an error.
  if (std::ranges::any_of(transactions_,
                          [](const Transaction& t) { return t.IsError(); })) {
    Complete(post_needed);
    return;
  }

  if (std::ranges::all_of(transactions_,
                          [](const Transaction& t) { return t.IsDone(); })) {
    Complete(post_needed);
    return;
  }
}

void HostResolverMdnsTask::Complete(bool post_needed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel any incomplete async transactions.
  for (auto& transaction : transactions_) {
    if (!transaction.IsDone())
      transaction.Cancel();
  }

  if (post_needed) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<HostResolverMdnsTask> task) {
                         if (task)
                           std::move(task->completion_closure_).Run();
                       },
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    std::move(completion_closure_).Run();
  }
}

}  // namespace net
