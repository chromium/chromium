// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_mdns_task.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"

namespace net {

class HostResolverMdnsTask::Transaction {
 public:
  Transaction(HostResolver::DnsQueryType query_type, HostResolverMdnsTask* task)
      : query_type_(query_type), result_(ERR_IO_PENDING), task_(task) {}

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);

    // Should not be completed or running yet.
    DCHECK_EQ(ERR_IO_PENDING, result_);
    DCHECK(!async_transaction_);

    uint16_t rrtype;
    switch (query_type_) {
      case net::HostResolver::DnsQueryType::A:
        rrtype = net::dns_protocol::kTypeA;
        break;
      case net::HostResolver::DnsQueryType::AAAA:
        rrtype = net::dns_protocol::kTypeAAAA;
        break;
      default:
        // Type not supported for MDNS.
        NOTREACHED();
        return;
    }

    // TODO(crbug.com/846423): Use |allow_cached_response| to set the
    // QUERY_CACHE flag or not.
    int flags = MDnsTransaction::SINGLE_RESULT | MDnsTransaction::QUERY_CACHE |
                MDnsTransaction::QUERY_NETWORK;
    // If |this| is destroyed, destruction of |internal_transaction_| should
    // cancel and prevent invocation of OnComplete.
    std::unique_ptr<MDnsTransaction> inner_transaction =
        task_->mdns_client_->CreateTransaction(
            rrtype, task_->hostname_, flags,
            base::BindRepeating(&HostResolverMdnsTask::Transaction::OnComplete,
                                base::Unretained(this)));

    // Side effect warning: Start() may finish and invoke callbacks inline.
    bool start_result = inner_transaction->Start();

    if (!start_result)
      task_->CompleteWithResult(ERR_FAILED, true /* post_needed */);
    else if (result_ == ERR_IO_PENDING)
      async_transaction_ = std::move(inner_transaction);
  }

  bool IsDone() const { return result_ != ERR_IO_PENDING; }
  bool IsError() const {
    return IsDone() && result_ != OK && result_ != ERR_NAME_NOT_RESOLVED;
  }
  int result() const { return result_; }

  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);
    DCHECK_EQ(ERR_IO_PENDING, result_);

    result_ = ERR_FAILED;
    async_transaction_ = nullptr;
  }

 private:
  void OnComplete(MDnsTransaction::Result result, const RecordParsed* parsed) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(task_->sequence_checker_);
    DCHECK_EQ(ERR_IO_PENDING, result_);

    switch (result) {
      case MDnsTransaction::RESULT_RECORD:
        result_ = OK;
        break;
      case MDnsTransaction::RESULT_NO_RESULTS:
      case MDnsTransaction::RESULT_NSEC:
        result_ = ERR_NAME_NOT_RESOLVED;
        break;
      default:
        // No other results should be possible with the request flags used.
        NOTREACHED();
    }

    if (result_ == net::OK) {
      switch (query_type_) {
        case net::HostResolver::DnsQueryType::A:
          task_->result_addresses_.push_back(
              IPEndPoint(parsed->rdata<net::ARecordRdata>()->address(), 0));
          break;
        case net::HostResolver::DnsQueryType::AAAA:
          task_->result_addresses_.push_back(
              IPEndPoint(parsed->rdata<net::AAAARecordRdata>()->address(), 0));
          break;
        default:
          NOTREACHED();
      }
    }

    // If we don't have a saved async_transaction, it means OnComplete was
    // invoked inline in MDnsTransaction::Start. Callbacks will need to be
    // invoked via post.
    task_->CheckCompletion(!async_transaction_);
  }

  const HostResolver::DnsQueryType query_type_;

  // ERR_IO_PENDING until transaction completes (or is cancelled).
  int result_;

  // Not saved until MDnsTransaction::Start completes to differentiate inline
  // completion.
  std::unique_ptr<MDnsTransaction> async_transaction_;

  // Back pointer. Expected to destroy |this| before destroying itself.
  HostResolverMdnsTask* const task_;
};

HostResolverMdnsTask::HostResolverMdnsTask(
    MDnsClient* mdns_client,
    const std::string& hostname,
    const std::vector<HostResolver::DnsQueryType>& query_types)
    : mdns_client_(mdns_client), hostname_(hostname), weak_ptr_factory_(this) {
  DCHECK(!query_types.empty());
  for (HostResolver::DnsQueryType query_type : query_types) {
    transactions_.emplace_back(query_type, this);
  }
}

HostResolverMdnsTask::~HostResolverMdnsTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transactions_.clear();
}

void HostResolverMdnsTask::Start(CompletionOnceCallback completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!completion_callback_);

  completion_callback_ = std::move(completion_callback);

  for (auto& transaction : transactions_) {
    // Only start transaction if it is not already marked done. A transaction
    // could be marked done before starting if it is preemptively canceled by
    // a previously started transaction finishing with an error.
    if (!transaction.IsDone())
      transaction.Start();
  }
}

void HostResolverMdnsTask::CheckCompletion(bool post_needed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Finish immediately if any transactions completed with an error.
  auto found_error =
      std::find_if(transactions_.begin(), transactions_.end(),
                   [](const Transaction& t) { return t.IsError(); });
  if (found_error != transactions_.end()) {
    CompleteWithResult(found_error->result(), post_needed);
    return;
  }

  if (std::all_of(transactions_.begin(), transactions_.end(),
                  [](const Transaction& t) { return t.IsDone(); })) {
    // Task is overall successful if any of the transactions found results.
    int result = result_addresses_.empty() ? ERR_NAME_NOT_RESOLVED : OK;

    CompleteWithResult(result, post_needed);
    return;
  }
}

void HostResolverMdnsTask::CompleteWithResult(int result, bool post_needed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel any incomplete async transactions.
  for (auto& transaction : transactions_) {
    if (!transaction.IsDone())
      transaction.Cancel();
  }

  if (post_needed) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<HostResolverMdnsTask> task, int result) {
              if (task)
                std::move(task->completion_callback_).Run(result);
            },
            weak_ptr_factory_.GetWeakPtr(), result));
  } else {
    std::move(completion_callback_).Run(result);
  }
}

}  // namespace net
