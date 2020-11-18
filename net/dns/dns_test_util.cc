// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_test_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_socket_allocator.h"
#include "net/dns/dns_util.h"
#include "net/dns/resolve_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const uint8_t kMalformedResponseHeader[] = {
    // Header
    0x00, 0x14,  // Arbitrary ID
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x01,  // 1 question
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs
};

// Create a response containing a valid question (as would normally be validated
// in DnsTransaction) but completely missing a header-declared answer.
DnsResponse CreateMalformedResponse(std::string hostname, uint16_t type) {
  std::string dns_name;
  CHECK(DNSDomainFromDot(hostname, &dns_name));
  DnsQuery query(0x14 /* id */, dns_name, type);

  // Build response to simulate the barebones validation DnsResponse applies to
  // responses received from the network.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(
      sizeof(kMalformedResponseHeader) + query.question().size());
  memcpy(buffer->data(), kMalformedResponseHeader,
         sizeof(kMalformedResponseHeader));
  memcpy(buffer->data() + sizeof(kMalformedResponseHeader),
         query.question().data(), query.question().size());

  DnsResponse response(buffer, buffer->size());
  CHECK(response.InitParseWithoutQuery(buffer->size()));

  return response;
}

class MockAddressSorter : public AddressSorter {
 public:
  ~MockAddressSorter() override = default;
  void Sort(const AddressList& list, CallbackType callback) const override {
    // Do nothing.
    std::move(callback).Run(true, list);
  }
};

}  // namespace

DnsResourceRecord BuildTestDnsRecord(std::string name,
                                     uint16_t type,
                                     std::string rdata,
                                     base::TimeDelta ttl) {
  DCHECK(!name.empty());

  DnsResourceRecord record;
  record.name = std::move(name);
  record.type = type;
  record.klass = dns_protocol::kClassIN;
  record.ttl = ttl.InSeconds();

  if (!rdata.empty())
    record.SetOwnedRdata(std::move(rdata));

  return record;
}

DnsResourceRecord BuildTestCnameRecord(std::string name,
                                       base::StringPiece canonical_name,
                                       base::TimeDelta ttl) {
  DCHECK(!name.empty());
  DCHECK(!canonical_name.empty());

  std::string rdata;
  CHECK(DNSDomainFromDot(canonical_name, &rdata));

  return BuildTestDnsRecord(std::move(name), dns_protocol::kTypeCNAME,
                            std::move(rdata), ttl);
}

DnsResourceRecord BuildTestAddressRecord(std::string name,
                                         const IPAddress& ip,
                                         base::TimeDelta ttl) {
  DCHECK(!name.empty());
  DCHECK(ip.IsValid());

  return BuildTestDnsRecord(
      std::move(name),
      ip.IsIPv4() ? dns_protocol::kTypeA : dns_protocol::kTypeAAAA,
      net::IPAddressToPackedString(ip), ttl);
}

DnsResourceRecord BuildTestTextRecord(std::string name,
                                      std::vector<std::string> text_strings,
                                      base::TimeDelta ttl) {
  DCHECK(!text_strings.empty());

  std::string rdata;
  for (const std::string& text_string : text_strings) {
    DCHECK(!text_string.empty());

    rdata += base::checked_cast<unsigned char>(text_string.size());
    rdata += text_string;
  }

  return BuildTestDnsRecord(std::move(name), dns_protocol::kTypeTXT,
                            std::move(rdata), ttl);
}

DnsResourceRecord BuildTestHttpsAliasRecord(std::string name,
                                            base::StringPiece alias_name,
                                            base::TimeDelta ttl) {
  DCHECK(!name.empty());

  std::string rdata("\000\000", 2);

  std::string alias_domain;
  CHECK(DNSDomainFromDot(alias_name, &alias_domain));
  rdata.append(alias_domain);

  return BuildTestDnsRecord(std::move(name), dns_protocol::kTypeHttps,
                            std::move(rdata), ttl);
}

DnsResourceRecord BuildTestHttpsServiceRecord(
    std::string name,
    uint16_t priority,
    base::StringPiece service_name,
    const std::map<uint16_t, std::string>& params,
    base::TimeDelta ttl) {
  DCHECK(!name.empty());
  DCHECK_NE(priority, 0);

  std::string rdata;

  char num_buffer[2];
  base::WriteBigEndian(num_buffer, priority);
  rdata.append(num_buffer, 2);

  std::string service_domain;
  CHECK(DNSDomainFromDot(service_name, &service_domain));
  rdata.append(service_domain);

  for (auto& param : params) {
    base::WriteBigEndian(num_buffer, param.first);
    rdata.append(num_buffer, 2);

    base::WriteBigEndian(num_buffer,
                         base::checked_cast<uint16_t>(param.second.size()));
    rdata.append(num_buffer, 2);

    rdata.append(param.second);
  }

  return BuildTestDnsRecord(std::move(name), dns_protocol::kTypeHttps,
                            std::move(rdata), ttl);
}

DnsResponse BuildTestDnsResponse(
    std::string name,
    uint16_t type,
    const std::vector<DnsResourceRecord>& answers,
    const std::vector<DnsResourceRecord>& authority,
    const std::vector<DnsResourceRecord>& additional,
    uint8_t rcode) {
  DCHECK(!name.empty());

  std::string dns_name;
  CHECK(DNSDomainFromDot(name, &dns_name));

  base::Optional<DnsQuery> query(base::in_place, 0, std::move(dns_name), type);
  return DnsResponse(0, true /* is_authoritative */, answers,
                     authority /* authority_records */,
                     additional /* additional_records */, query, rcode,
                     false /* validate_records */);
}

DnsResponse BuildTestDnsAddressResponse(std::string name,
                                        const IPAddress& ip,
                                        std::string answer_name) {
  DCHECK(ip.IsValid());

  if (answer_name.empty())
    answer_name = name;

  std::vector<DnsResourceRecord> answers = {
      BuildTestAddressRecord(std::move(answer_name), ip)};

  return BuildTestDnsResponse(
      std::move(name),
      ip.IsIPv4() ? dns_protocol::kTypeA : dns_protocol::kTypeAAAA, answers);
}

DnsResponse BuildTestDnsAddressResponseWithCname(std::string name,
                                                 const IPAddress& ip,
                                                 std::string cannonname,
                                                 std::string answer_name) {
  DCHECK(ip.IsValid());
  DCHECK(!cannonname.empty());

  if (answer_name.empty())
    answer_name = name;

  std::string cname_rdata;
  CHECK(DNSDomainFromDot(cannonname, &cname_rdata));

  std::vector<DnsResourceRecord> answers = {
      BuildTestDnsRecord(std::move(answer_name), dns_protocol::kTypeCNAME,
                         std::move(cname_rdata)),
      BuildTestAddressRecord(std::move(cannonname), ip)};

  return BuildTestDnsResponse(
      std::move(name),
      ip.IsIPv4() ? dns_protocol::kTypeA : dns_protocol::kTypeAAAA, answers);
}

DnsResponse BuildTestDnsTextResponse(
    std::string name,
    std::vector<std::vector<std::string>> text_records,
    std::string answer_name) {
  if (answer_name.empty())
    answer_name = name;

  std::vector<DnsResourceRecord> answers;
  for (std::vector<std::string>& text_record : text_records) {
    answers.push_back(BuildTestTextRecord(answer_name, std::move(text_record)));
  }

  return BuildTestDnsResponse(std::move(name), dns_protocol::kTypeTXT, answers);
}

DnsResponse BuildTestDnsPointerResponse(std::string name,
                                        std::vector<std::string> pointer_names,
                                        std::string answer_name) {
  if (answer_name.empty())
    answer_name = name;

  std::vector<DnsResourceRecord> answers;
  for (std::string& pointer_name : pointer_names) {
    std::string rdata;
    CHECK(DNSDomainFromDot(pointer_name, &rdata));

    answers.push_back(BuildTestDnsRecord(answer_name, dns_protocol::kTypePTR,
                                         std::move(rdata)));
  }

  return BuildTestDnsResponse(std::move(name), dns_protocol::kTypePTR, answers);
}

DnsResponse BuildTestDnsServiceResponse(
    std::string name,
    std::vector<TestServiceRecord> service_records,
    std::string answer_name) {
  if (answer_name.empty())
    answer_name = name;

  std::vector<DnsResourceRecord> answers;
  for (TestServiceRecord& service_record : service_records) {
    std::string rdata;
    char num_buffer[2];
    base::WriteBigEndian(num_buffer, service_record.priority);
    rdata.append(num_buffer, 2);
    base::WriteBigEndian(num_buffer, service_record.weight);
    rdata.append(num_buffer, 2);
    base::WriteBigEndian(num_buffer, service_record.port);
    rdata.append(num_buffer, 2);
    std::string dns_name;
    CHECK(DNSDomainFromDot(service_record.target, &dns_name));
    rdata += dns_name;

    answers.push_back(BuildTestDnsRecord(answer_name, dns_protocol::kTypeSRV,
                                         std::move(rdata),
                                         base::TimeDelta::FromHours(5)));
  }

  return BuildTestDnsResponse(std::move(name), dns_protocol::kTypeSRV, answers);
}

MockDnsClientRule::Result::Result(ResultType type,
                                  base::Optional<DnsResponse> response)
    : type(type), response(std::move(response)) {}

MockDnsClientRule::Result::Result(DnsResponse response)
    : type(OK), response(std::move(response)) {}

MockDnsClientRule::Result::Result(Result&& result) = default;

MockDnsClientRule::Result::~Result() = default;

MockDnsClientRule::Result& MockDnsClientRule::Result::operator=(
    Result&& result) = default;

MockDnsClientRule::MockDnsClientRule(const std::string& prefix,
                                     uint16_t qtype,
                                     bool secure,
                                     Result result,
                                     bool delay,
                                     URLRequestContext* context)
    : result(std::move(result)),
      prefix(prefix),
      qtype(qtype),
      secure(secure),
      delay(delay),
      context(context) {}

MockDnsClientRule::MockDnsClientRule(MockDnsClientRule&& rule) = default;

// A DnsTransaction which uses MockDnsClientRuleList to determine the response.
class MockDnsTransactionFactory::MockTransaction
    : public DnsTransaction,
      public base::SupportsWeakPtr<MockTransaction> {
 public:
  MockTransaction(const MockDnsClientRuleList& rules,
                  const std::string& hostname,
                  uint16_t qtype,
                  bool secure,
                  bool force_doh_server_available,
                  SecureDnsMode secure_dns_mode,
                  ResolveContext* resolve_context,
                  bool fast_timeout,
                  DnsTransactionFactory::CallbackType callback)
      : result_(MockDnsClientRule::FAIL),
        hostname_(hostname),
        qtype_(qtype),
        callback_(std::move(callback)),
        started_(false),
        delayed_(false) {
    // Do not allow matching any rules if transaction is secure and no DoH
    // servers are available.
    if (!secure || force_doh_server_available ||
        resolve_context->NumAvailableDohServers(
            resolve_context->current_session_for_testing()) > 0) {
      // Find the relevant rule which matches |qtype|, |secure|, prefix of
      // |hostname|, and |url_request_context| (iff the rule context is not
      // null).
      for (size_t i = 0; i < rules.size(); ++i) {
        const std::string& prefix = rules[i].prefix;
        if ((rules[i].qtype == qtype) && (rules[i].secure == secure) &&
            (hostname.size() >= prefix.size()) &&
            (hostname.compare(0, prefix.size(), prefix) == 0) &&
            (!rules[i].context ||
             rules[i].context == resolve_context->url_request_context())) {
          const MockDnsClientRule::Result* result = &rules[i].result;
          result_ = MockDnsClientRule::Result(result->type);
          delayed_ = rules[i].delay;

          // Generate a DnsResponse when not provided with the rule.
          std::vector<DnsResourceRecord> authority_records;
          std::string dns_name;
          CHECK(DNSDomainFromDot(hostname_, &dns_name));
          base::Optional<DnsQuery> query(base::in_place, 22 /* id */, dns_name,
                                         qtype_);
          switch (result->type) {
            case MockDnsClientRule::NODOMAIN:
            case MockDnsClientRule::EMPTY:
              DCHECK(!result->response);  // Not expected to be provided.
              authority_records = {BuildTestDnsRecord(
                  hostname_, dns_protocol::kTypeSOA, "fake rdata")};
              result_.response = DnsResponse(
                  22 /* id */, false /* is_authoritative */,
                  std::vector<DnsResourceRecord>() /* answers */,
                  authority_records,
                  std::vector<DnsResourceRecord>() /* additional_records */,
                  query,
                  result->type == MockDnsClientRule::NODOMAIN
                      ? dns_protocol::kRcodeNXDOMAIN
                      : 0);
              break;
            case MockDnsClientRule::FAIL:
            case MockDnsClientRule::TIMEOUT:
              DCHECK(!result->response);  // Not expected to be provided.
              break;
            case MockDnsClientRule::SLOW:
              if (!fast_timeout)
                SetResponse(result);
              break;
            case MockDnsClientRule::OK:
              SetResponse(result);
              break;
            case MockDnsClientRule::MALFORMED:
              DCHECK(!result->response);  // Not expected to be provided.
              result_.response = CreateMalformedResponse(hostname_, qtype_);
              break;
          }

          break;
        }
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
        FROM_HERE, base::BindOnce(&MockTransaction::Finish, AsWeakPtr()));
  }

  void FinishDelayedTransaction() {
    EXPECT_TRUE(delayed_);
    delayed_ = false;
    Finish();
  }

  bool delayed() const { return delayed_; }

 private:
  void SetResponse(const MockDnsClientRule::Result* result) {
    if (result->response) {
      // Copy response in case |result| is destroyed before the transaction
      // completes.
      auto buffer_copy =
          base::MakeRefCounted<IOBuffer>(result->response->io_buffer_size());
      memcpy(buffer_copy->data(), result->response->io_buffer()->data(),
             result->response->io_buffer_size());
      result_.response = DnsResponse(std::move(buffer_copy),
                                     result->response->io_buffer_size());
      CHECK(result_.response->InitParseWithoutQuery(
          result->response->io_buffer_size()));
    } else {
      // Generated response only available for address types.
      DCHECK(qtype_ == dns_protocol::kTypeA ||
             qtype_ == dns_protocol::kTypeAAAA);
      result_.response = BuildTestDnsAddressResponse(
          hostname_, qtype_ == dns_protocol::kTypeA
                         ? IPAddress::IPv4Localhost()
                         : IPAddress::IPv6Localhost());
    }
  }

  void Finish() {
    switch (result_.type) {
      case MockDnsClientRule::NODOMAIN:
      case MockDnsClientRule::FAIL:
        std::move(callback_).Run(
            this, ERR_NAME_NOT_RESOLVED,
            result_.response ? &result_.response.value() : nullptr,
            base::nullopt);
        break;
      case MockDnsClientRule::EMPTY:
      case MockDnsClientRule::OK:
      case MockDnsClientRule::MALFORMED:
        std::move(callback_).Run(
            this, OK, result_.response ? &result_.response.value() : nullptr,
            base::nullopt);
        break;
      case MockDnsClientRule::TIMEOUT:
        std::move(callback_).Run(this, ERR_DNS_TIMED_OUT, nullptr,
                                 base::nullopt);
        break;
      case MockDnsClientRule::SLOW:
        if (result_.response) {
          std::move(callback_).Run(
              this, OK, result_.response ? &result_.response.value() : nullptr,
              base::nullopt);
        } else {
          std::move(callback_).Run(this, ERR_DNS_TIMED_OUT, nullptr,
                                   base::nullopt);
        }
    }
  }

  void SetRequestPriority(RequestPriority priority) override {}

  MockDnsClientRule::Result result_;
  const std::string hostname_;
  const uint16_t qtype_;
  DnsTransactionFactory::CallbackType callback_;
  bool started_;
  bool delayed_;
};

class MockDnsTransactionFactory::MockDohProbeRunner : public DnsProbeRunner {
 public:
  explicit MockDohProbeRunner(base::WeakPtr<MockDnsTransactionFactory> factory)
      : factory_(std::move(factory)) {}

  ~MockDohProbeRunner() override {
    if (factory_)
      factory_->running_doh_probe_runners_.erase(this);
  }

  void Start(bool network_change) override {
    DCHECK(factory_);
    factory_->running_doh_probe_runners_.insert(this);
  }

  base::TimeDelta GetDelayUntilNextProbeForTest(
      size_t doh_server_index) const override {
    NOTREACHED();
    return base::TimeDelta();
  }

 private:
  base::WeakPtr<MockDnsTransactionFactory> factory_;
};

MockDnsTransactionFactory::MockDnsTransactionFactory(
    MockDnsClientRuleList rules)
    : rules_(std::move(rules)) {}

MockDnsTransactionFactory::~MockDnsTransactionFactory() = default;

std::unique_ptr<DnsTransaction> MockDnsTransactionFactory::CreateTransaction(
    const std::string& hostname,
    uint16_t qtype,
    DnsTransactionFactory::CallbackType callback,
    const NetLogWithSource&,
    bool secure,
    SecureDnsMode secure_dns_mode,
    ResolveContext* resolve_context,
    bool fast_timeout) {
  std::unique_ptr<MockTransaction> transaction =
      std::make_unique<MockTransaction>(
          rules_, hostname, qtype, secure, force_doh_server_available_,
          secure_dns_mode, resolve_context, fast_timeout, std::move(callback));
  if (transaction->delayed())
    delayed_transactions_.push_back(transaction->AsWeakPtr());
  return transaction;
}

std::unique_ptr<DnsProbeRunner> MockDnsTransactionFactory::CreateDohProbeRunner(
    ResolveContext* resolve_context) {
  return std::make_unique<MockDohProbeRunner>(weak_ptr_factory_.GetWeakPtr());
}

void MockDnsTransactionFactory::AddEDNSOption(const OptRecordRdata::Opt& opt) {}

SecureDnsMode MockDnsTransactionFactory::GetSecureDnsModeForTest() {
  return SecureDnsMode::kAutomatic;
}

void MockDnsTransactionFactory::CompleteDelayedTransactions() {
  DelayedTransactionList old_delayed_transactions;
  old_delayed_transactions.swap(delayed_transactions_);
  for (auto it = old_delayed_transactions.begin();
       it != old_delayed_transactions.end(); ++it) {
    if (it->get())
      (*it)->FinishDelayedTransaction();
  }
}

bool MockDnsTransactionFactory::CompleteOneDelayedTransactionOfType(
    DnsQueryType type) {
  for (base::WeakPtr<MockTransaction>& t : delayed_transactions_) {
    if (t && t->GetType() == DnsQueryTypeToQtype(type)) {
      t->FinishDelayedTransaction();
      t.reset();
      return true;
    }
  }
  return false;
}

MockDnsClient::MockDnsClient(DnsConfig config, MockDnsClientRuleList rules)
    : config_(std::move(config)),
      factory_(new MockDnsTransactionFactory(std::move(rules))),
      address_sorter_(new MockAddressSorter()) {
  effective_config_ = BuildEffectiveConfig();
  session_ = BuildSession();
}

MockDnsClient::~MockDnsClient() = default;

bool MockDnsClient::CanUseSecureDnsTransactions() const {
  const DnsConfig* config = GetEffectiveConfig();
  return config && config->IsValid() && !config->dns_over_https_servers.empty();
}

bool MockDnsClient::CanUseInsecureDnsTransactions() const {
  const DnsConfig* config = GetEffectiveConfig();
  return config && config->IsValid() && insecure_enabled_ &&
         !config->dns_over_tls_active;
}

void MockDnsClient::SetInsecureEnabled(bool enabled) {
  insecure_enabled_ = enabled;
}

bool MockDnsClient::FallbackFromSecureTransactionPreferred(
    ResolveContext* context) const {
  bool doh_server_available =
      force_doh_server_available_ ||
      context->NumAvailableDohServers(session_.get()) > 0;
  return !CanUseSecureDnsTransactions() || !doh_server_available;
}

bool MockDnsClient::FallbackFromInsecureTransactionPreferred() const {
  return !CanUseInsecureDnsTransactions() ||
         fallback_failures_ >= max_fallback_failures_;
}

bool MockDnsClient::SetSystemConfig(base::Optional<DnsConfig> system_config) {
  if (ignore_system_config_changes_)
    return false;

  base::Optional<DnsConfig> before = effective_config_;
  config_ = std::move(system_config);
  effective_config_ = BuildEffectiveConfig();
  session_ = BuildSession();
  return before != effective_config_;
}

bool MockDnsClient::SetConfigOverrides(DnsConfigOverrides config_overrides) {
  base::Optional<DnsConfig> before = effective_config_;
  overrides_ = std::move(config_overrides);
  effective_config_ = BuildEffectiveConfig();
  session_ = BuildSession();
  return before != effective_config_;
}

void MockDnsClient::ReplaceCurrentSession() {
  // Noop if no current effective config.
  session_ = BuildSession();
}

DnsSession* MockDnsClient::GetCurrentSession() {
  return session_.get();
}

const DnsConfig* MockDnsClient::GetEffectiveConfig() const {
  return effective_config_.has_value() ? &effective_config_.value() : nullptr;
}

const DnsHosts* MockDnsClient::GetHosts() const {
  const DnsConfig* config = GetEffectiveConfig();
  if (!config)
    return nullptr;

  return &config->hosts;
}

DnsTransactionFactory* MockDnsClient::GetTransactionFactory() {
  return GetEffectiveConfig() ? factory_.get() : nullptr;
}

AddressSorter* MockDnsClient::GetAddressSorter() {
  return GetEffectiveConfig() ? address_sorter_.get() : nullptr;
}

void MockDnsClient::IncrementInsecureFallbackFailures() {
  ++fallback_failures_;
}

void MockDnsClient::ClearInsecureFallbackFailures() {
  fallback_failures_ = 0;
}

base::Optional<DnsConfig> MockDnsClient::GetSystemConfigForTesting() const {
  return config_;
}

DnsConfigOverrides MockDnsClient::GetConfigOverridesForTesting() const {
  return overrides_;
}

void MockDnsClient::SetTransactionFactoryForTesting(
    std::unique_ptr<DnsTransactionFactory> factory) {
  NOTREACHED();
}

void MockDnsClient::CompleteDelayedTransactions() {
  factory_->CompleteDelayedTransactions();
}

bool MockDnsClient::CompleteOneDelayedTransactionOfType(DnsQueryType type) {
  return factory_->CompleteOneDelayedTransactionOfType(type);
}

void MockDnsClient::SetForceDohServerAvailable(bool available) {
  force_doh_server_available_ = available;
  factory_->set_force_doh_server_available(available);
}

base::Optional<DnsConfig> MockDnsClient::BuildEffectiveConfig() {
  if (overrides_.OverridesEverything())
    return overrides_.ApplyOverrides(DnsConfig());
  if (!config_ || !config_.value().IsValid())
    return base::nullopt;

  return overrides_.ApplyOverrides(config_.value());
}

scoped_refptr<DnsSession> MockDnsClient::BuildSession() {
  if (!effective_config_)
    return nullptr;

  // Session not expected to be used for anything that will actually require
  // random numbers.
  auto null_random_callback =
      base::BindRepeating([](int, int) -> int { IMMEDIATE_CRASH(); });

  auto socket_allocator = std::make_unique<DnsSocketAllocator>(
      &socket_factory_, effective_config_.value().nameservers,
      nullptr /* net_log */);

  return base::MakeRefCounted<DnsSession>(
      effective_config_.value(), std::move(socket_allocator),
      null_random_callback, nullptr /* net_log */);
}

}  // namespace net
