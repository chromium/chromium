// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_test_util.h"

#include <string>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class MockAddressSorter : public AddressSorter {
 public:
  ~MockAddressSorter() override = default;
  void Sort(const AddressList& list, CallbackType callback) const override {
    // Do nothing.
    std::move(callback).Run(true, list);
  }
};

// A DnsTransaction which uses MockDnsClientRuleList to determine the response.
class MockTransaction : public DnsTransaction,
                        public base::SupportsWeakPtr<MockTransaction> {
 public:
  MockTransaction(const MockDnsClientRuleList& rules,
                  const std::string& hostname,
                  uint16_t qtype,
                  DnsTransactionFactory::CallbackType callback)
      : result_(MockDnsClientRule::FAIL),
        hostname_(hostname),
        qtype_(qtype),
        callback_(std::move(callback)),
        started_(false),
        delayed_(false) {
    // Find the relevant rule which matches |qtype| and prefix of |hostname|.
    for (size_t i = 0; i < rules.size(); ++i) {
      const std::string& prefix = rules[i].prefix;
      if ((rules[i].qtype == qtype) &&
          (hostname.size() >= prefix.size()) &&
          (hostname.compare(0, prefix.size(), prefix) == 0)) {
        result_ = rules[i].result;
        delayed_ = rules[i].delay;

        // Fill in an IP address for the result if one was not specified.
        if (result_.ip.empty() && result_.type == MockDnsClientRule::OK) {
          result_.ip = qtype_ == dns_protocol::kTypeA
                           ? IPAddress::IPv4Localhost()
                           : IPAddress::IPv6Localhost();
        }
        break;
      }
    }
  }

  const std::string& GetHostname() const override { return hostname_; }

  uint16_t GetType() const override { return qtype_; }

  void Start() override {
    EXPECT_FALSE(started_);
    started_ = true;
    if (delayed_)
      return;
    // Using WeakPtr to cleanly cancel when transaction is destroyed.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&MockTransaction::Finish, AsWeakPtr()));
  }

  void FinishDelayedTransaction() {
    EXPECT_TRUE(delayed_);
    delayed_ = false;
    Finish();
  }

  bool delayed() const { return delayed_; }

 private:
  void Finish() {
    switch (result_.type) {
      case MockDnsClientRule::NODOMAIN:
      case MockDnsClientRule::EMPTY:
      case MockDnsClientRule::OK: {
        std::string qname;
        DNSDomainFromDot(hostname_, &qname);
        DnsQuery query(0, qname, qtype_);

        DnsResponse response;
        char* buffer = response.io_buffer()->data();
        int nbytes = query.io_buffer()->size();
        memcpy(buffer, query.io_buffer()->data(), nbytes);
        dns_protocol::Header* header =
            reinterpret_cast<dns_protocol::Header*>(buffer);
        header->flags |= base::HostToNet16(dns_protocol::kFlagResponse);
        if (MockDnsClientRule::NODOMAIN == result_.type) {
          header->flags |= base::HostToNet16(dns_protocol::kRcodeNXDOMAIN);
        }

        const uint16_t kPointerToQueryName =
            static_cast<uint16_t>(0xc000 | sizeof(*header));

        const uint32_t kTTL = 86400;  // One day.

        // Size of RDATA which is a IPv4 or IPv6 address.
        if (MockDnsClientRule::OK == result_.type)
          EXPECT_TRUE(result_.ip.IsValid());
        size_t rdata_size = result_.ip.size();

        if (MockDnsClientRule::OK == result_.type) {
          // Write the answer using the expected IP address.
          // 12 is the sum of sizes of the compressed name reference, TYPE,
          // CLASS, TTL and RDLENGTH.
          size_t answer_size = 12 + rdata_size;
          uint16_t answer_count = 1;

          // Compressed name reference for the owner of the current RR.
          uint16_t last_owner_ptr = kPointerToQueryName;

          std::string cname_as_labels;

          // There are two modes of operation distinguished based on whether
          // |result_.canonical_name| is empty or not. If it's empty, then the
          // resource records presented are of the form:
          //
          //     <question>       86400 IN <question type> <answer>
          //
          // However, if the canonical name is not empty, then:
          //
          //     <question>       86400 IN CNAME <canonical name>
          //     <canonical name> 86400 IN <question type> <answer>
          if (!result_.canonical_name.empty()) {
            DNSDomainFromDot(result_.canonical_name, &cname_as_labels);
            EXPECT_FALSE(cname_as_labels.empty());
            answer_size += 12 + cname_as_labels.size();
            ++answer_count;
          }
          header->ancount = base::HostToNet16(answer_count);

          base::BigEndianWriter writer(buffer + nbytes, answer_size);

          // CNAME record goes first.
          if (!result_.canonical_name.empty()) {
            writer.WriteU16(last_owner_ptr);
            writer.WriteU16(dns_protocol::kTypeCNAME);
            writer.WriteU16(dns_protocol::kClassIN);
            writer.WriteU32(kTTL);
            writer.WriteU16(static_cast<uint16_t>(cname_as_labels.size()));
            writer.WriteBytes(cname_as_labels.data(), cname_as_labels.size());
            last_owner_ptr = static_cast<uint16_t>(0xc000 | (nbytes + 12));
          }

          writer.WriteU16(last_owner_ptr);
          writer.WriteU16(qtype_);
          writer.WriteU16(dns_protocol::kClassIN);
          writer.WriteU32(kTTL);
          writer.WriteU16(static_cast<uint16_t>(rdata_size));
          writer.WriteBytes(result_.ip.bytes().data(), rdata_size);

          nbytes += answer_size;
        } else {
          // authority will be 12 bytes.
          size_t authority_size = 12;
          header->ancount = base::HostToNet16(0);
          header->nscount = base::HostToNet16(1);
          base::BigEndianWriter writer(buffer + nbytes, authority_size);
          writer.WriteU16(kPointerToQueryName);
          writer.WriteU16(dns_protocol::kTypeSOA);
          writer.WriteU16(dns_protocol::kClassIN);
          writer.WriteU32(kTTL);
          writer.WriteU16(static_cast<uint16_t>(rdata_size));
          nbytes += authority_size;
        }
        EXPECT_TRUE(response.InitParse(nbytes, query));
        if (MockDnsClientRule::NODOMAIN == result_.type) {
          std::move(callback_).Run(this, ERR_NAME_NOT_RESOLVED, &response);
        } else {
          std::move(callback_).Run(this, OK, &response);
        }
      } break;
      case MockDnsClientRule::FAIL:
        std::move(callback_).Run(this, ERR_NAME_NOT_RESOLVED, NULL);
        break;
      case MockDnsClientRule::TIMEOUT:
        std::move(callback_).Run(this, ERR_DNS_TIMED_OUT, NULL);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void SetRequestContext(URLRequestContext*) override {}
  void SetRequestPriority(RequestPriority priority) override {}

  MockDnsClientRule::Result result_;
  const std::string hostname_;
  const uint16_t qtype_;
  DnsTransactionFactory::CallbackType callback_;
  bool started_;
  bool delayed_;
};

}  // namespace

// A DnsTransactionFactory which creates MockTransaction.
class MockTransactionFactory : public DnsTransactionFactory {
 public:
  explicit MockTransactionFactory(const MockDnsClientRuleList& rules)
      : rules_(rules) {}

  ~MockTransactionFactory() override = default;

  std::unique_ptr<DnsTransaction> CreateTransaction(
      const std::string& hostname,
      uint16_t qtype,
      DnsTransactionFactory::CallbackType callback,
      const NetLogWithSource&) override {
    std::unique_ptr<MockTransaction> transaction =
        std::make_unique<MockTransaction>(rules_, hostname, qtype,
                                          std::move(callback));
    if (transaction->delayed())
      delayed_transactions_.push_back(transaction->AsWeakPtr());
    return transaction;
  }

  void AddEDNSOption(const OptRecordRdata::Opt& opt) override {
    NOTREACHED() << "Not implemented";
  }

  void CompleteDelayedTransactions() {
    DelayedTransactionList old_delayed_transactions;
    old_delayed_transactions.swap(delayed_transactions_);
    for (auto it = old_delayed_transactions.begin();
         it != old_delayed_transactions.end(); ++it) {
      if (it->get())
        (*it)->FinishDelayedTransaction();
    }
  }

 private:
  typedef std::vector<base::WeakPtr<MockTransaction> > DelayedTransactionList;

  MockDnsClientRuleList rules_;
  DelayedTransactionList delayed_transactions_;
};

MockDnsClient::MockDnsClient(const DnsConfig& config,
                             const MockDnsClientRuleList& rules)
      : config_(config),
        factory_(new MockTransactionFactory(rules)),
        address_sorter_(new MockAddressSorter()) {
}

MockDnsClient::~MockDnsClient() = default;

void MockDnsClient::SetConfig(const DnsConfig& config) {
  config_ = config;
}

const DnsConfig* MockDnsClient::GetConfig() const {
  return config_.IsValid() ? &config_ : NULL;
}

DnsTransactionFactory* MockDnsClient::GetTransactionFactory() {
  return config_.IsValid() ? factory_.get() : NULL;
}

AddressSorter* MockDnsClient::GetAddressSorter() {
  return address_sorter_.get();
}

void MockDnsClient::CompleteDelayedTransactions() {
  factory_->CompleteDelayedTransactions();
}

}  // namespace net
