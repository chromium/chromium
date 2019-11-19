// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_mdns_task.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"

namespace net {

namespace {
HostCache::Entry ParseHostnameResult(const std::string& host, uint16_t port) {
  // Filter out root domain. Depending on the type, it either means no-result
  // or is simply not a result important to any expected Chrome usecases.
  if (host.empty()) {
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }
  return HostCache::Entry(OK,
                          std::vector<HostPortPair>({HostPortPair(host, port)}),
                          HostCache::Entry::SOURCE_UNKNOWN);
}
}  // namespace

class HostResolverMdnsTask::Transaction {
 public:
  Transaction(DnsQueryType query_type, HostResolverMdnsTask* task)
      : query_type_(query_type),
        results_(ERR_IO_PENDING, HostCache::Entry::SOURCE_UNKNOWN),
        task_(task) {}

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);

    // Should not be completed or running yet.
    DCHECK_EQ(ERR_IO_PENDING, results_.error());
    DCHECK(!async_transaction_);

    // TODO(crbug.com/926300): Use |allow_cached_response| to set the
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

    if (!start_result)
      task_->Complete(true /* post_needed */);
    else if (results_.error() == ERR_IO_PENDING)
      async_transaction_ = std::move(inner_transaction);
  }

  bool IsDone() const { return results_.error() != ERR_IO_PENDING; }
  bool IsError() const {
    return IsDone() && results_.error() != OK &&
           results_.error() != ERR_NAME_NOT_RESOLVED;
  }
  const HostCache::Entry& results() const { return results_; }

  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);
    DCHECK_EQ(ERR_IO_PENDING, results_.error());

    results_ = HostCache::Entry(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
    async_transaction_ = nullptr;
  }

 private:
  void OnComplete(MDnsTransaction::Result result, const RecordParsed* parsed) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);
    DCHECK_EQ(ERR_IO_PENDING, results_.error());

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

    results_ = HostResolverMdnsTask::ParseResult(error, query_type_, parsed,
                                                 task_->hostname_);

    // If we don't have a saved async_transaction, it means OnComplete was
    // invoked inline in MDnsTransaction::Start. Callbacks will need to be
    // invoked via post.
    task_->CheckCompletion(!async_transaction_);
  }

  const DnsQueryType query_type_;

  // ERR_IO_PENDING until transaction completes (or is cancelled).
  HostCache::Entry results_;

  // Not saved until MDnsTransaction::Start completes to differentiate inline
  // completion.
  std::unique_ptr<MDnsTransaction> async_transaction_;

  // Back pointer. Expected to destroy |this| before destroying itself.
  HostResolverMdnsTask* const task_;
};

HostResolverMdnsTask::HostResolverMdnsTask(
    MDnsClient* mdns_client,
    const std::string& hostname,
    const std::vector<DnsQueryType>& query_types)
    : mdns_client_(mdns_client), hostname_(hostname) {
  DCHECK(!query_types.empty());
  for (DnsQueryType query_type : query_types) {
    transactions_.emplace_back(query_type, this);
  }
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

HostCache::Entry HostResolverMdnsTask::GetResults() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!transactions_.empty());
  DCHECK(!completion_closure_);
  DCHECK(std::all_of(transactions_.begin(), transactions_.end(),
                     [](const Transaction& t) { return t.IsDone(); }));

  auto found_error =
      std::find_if(transactions_.begin(), transactions_.end(),
                   [](const Transaction& t) { return t.IsError(); });
  if (found_error != transactions_.end()) {
    return found_error->results();
  }

  HostCache::Entry combined_results = transactions_.front().results();
  for (auto it = ++transactions_.begin(); it != transactions_.end(); ++it) {
    combined_results = HostCache::Entry::MergeEntries(
        std::move(combined_results), it->results());
  }

  return combined_results;
}

// static
HostCache::Entry HostResolverMdnsTask::ParseResult(
    int error,
    DnsQueryType query_type,
    const RecordParsed* parsed,
    const std::string& expected_hostname) {
  if (error != OK) {
    return HostCache::Entry(error, HostCache::Entry::SOURCE_UNKNOWN);
  }
  DCHECK(parsed);

  // Expected to be validated by MDnsClient.
  DCHECK_EQ(DnsQueryTypeToQtype(query_type), parsed->type());
  DCHECK(base::EqualsCaseInsensitiveASCII(expected_hostname, parsed->name()));

  switch (query_type) {
    case DnsQueryType::UNSPECIFIED:
      // Should create two separate transactions with specified type.
    case DnsQueryType::ESNI:
      // ESNI queries are not expected to be useful in mDNS, so they're not
      // supported.
      NOTREACHED();
      return HostCache::Entry(ERR_FAILED, HostCache::Entry::SOURCE_UNKNOWN);
    case DnsQueryType::A:
      return HostCache::Entry(
          OK,
          AddressList(
              IPEndPoint(parsed->rdata<net::ARecordRdata>()->address(), 0)),
          HostCache::Entry::SOURCE_UNKNOWN);
    case DnsQueryType::AAAA:
      return HostCache::Entry(
          OK,
          AddressList(
              IPEndPoint(parsed->rdata<net::AAAARecordRdata>()->address(), 0)),
          HostCache::Entry::SOURCE_UNKNOWN);
    case DnsQueryType::TXT:
      return HostCache::Entry(OK, parsed->rdata<net::TxtRecordRdata>()->texts(),
                              HostCache::Entry::SOURCE_UNKNOWN);
    case DnsQueryType::PTR:
      return ParseHostnameResult(parsed->rdata<PtrRecordRdata>()->ptrdomain(),
                                 0 /* port */);
    case DnsQueryType::SRV:
      return ParseHostnameResult(parsed->rdata<SrvRecordRdata>()->target(),
                                 parsed->rdata<SrvRecordRdata>()->port());
  }
}

void HostResolverMdnsTask::CheckCompletion(bool post_needed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Finish immediately if any transactions completed with an error.
  if (std::any_of(transactions_.begin(), transactions_.end(),
                  [](const Transaction& t) { return t.IsError(); })) {
    Complete(post_needed);
    return;
  }

  if (std::all_of(transactions_.begin(), transactions_.end(),
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
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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
