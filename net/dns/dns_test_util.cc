// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_test_util.h"

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
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
std::unique_ptr<DnsResponse> CreateMalformedResponse(std::string hostname,
                                                     uint16_t type) {
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

  auto response = std::make_unique<DnsResponse>(buffer, buffer->size());
  CHECK(response->InitParseWithoutQuery(buffer->size()));

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

DnsResourceRecord BuildCannonnameRecord(std::string name,
                                        std::string cannonname) {
  DCHECK(!name.empty());
  DCHECK(!cannonname.empty());

  DnsResourceRecord record;
  record.name = std::move(name);
  record.type = dns_protocol::kTypeCNAME;
  record.klass = dns_protocol::kClassIN;
  record.ttl = base::TimeDelta::FromDays(1).InSeconds();
  CHECK(DNSDomainFromDot(cannonname, &record.owned_rdata));
  record.rdata = record.owned_rdata;

  return record;
}

// Note: This is not a fully compliant SOA record, merely the bare amount needed
// in DnsRecord::ParseToAddressList() processessing. This record will not pass
// RecordParsed validation.
DnsResourceRecord BuildSoaRecord(std::string name) {
  DCHECK(!name.empty());

  DnsResourceRecord record;
  record.name = std::move(name);
  record.type = dns_protocol::kTypeSOA;
  record.klass = dns_protocol::kClassIN;
  record.ttl = base::TimeDelta::FromDays(1).InSeconds();
  record.SetOwnedRdata("fake_rdata");

  return record;
}

DnsResourceRecord BuildTextRecord(std::string name,
                                  std::vector<std::string> text_strings) {
  DCHECK(!name.empty());
  DCHECK(!text_strings.empty());

  DnsResourceRecord record;
  record.name = std::move(name);
  record.type = dns_protocol::kTypeTXT;
  record.klass = dns_protocol::kClassIN;
  record.ttl = base::TimeDelta::FromDays(1).InSeconds();

  std::string rdata;
  for (std::string text_string : text_strings) {
    DCHECK(!text_string.empty());

    rdata += base::checked_cast<unsigned char>(text_string.size());
    rdata += std::move(text_string);
  }
  record.SetOwnedRdata(std::move(rdata));

  return record;
}

DnsResourceRecord BuildPointerRecord(std::string name,
                                     std::string pointer_name) {
  DCHECK(!name.empty());
  DCHECK(!pointer_name.empty());

  DnsResourceRecord record;
  record.name = std::move(name);
  record.type = dns_protocol::kTypePTR;
  record.klass = dns_protocol::kClassIN;
  record.ttl = base::TimeDelta::FromDays(1).InSeconds();
  CHECK(DNSDomainFromDot(pointer_name, &record.owned_rdata));
  record.rdata = record.owned_rdata;

  return record;
}

DnsResourceRecord BuildServiceRecord(std::string name,
                                     TestServiceRecord service) {
  DCHECK(!name.empty());
  DCHECK(!service.target.empty());

  DnsResourceRecord record;
  record.name = std::move(name);
  record.type = dns_protocol::kTypeSRV;
  record.klass = dns_protocol::kClassIN;
  record.ttl = base::TimeDelta::FromHours(5).InSeconds();

  std::string rdata;
  char num_buffer[2];
  base::WriteBigEndian(num_buffer, service.priority);
  rdata.append(num_buffer, 2);
  base::WriteBigEndian(num_buffer, service.weight);
  rdata.append(num_buffer, 2);
  base::WriteBigEndian(num_buffer, service.port);
  rdata.append(num_buffer, 2);
  std::string dns_name;
  CHECK(DNSDomainFromDot(service.target, &dns_name));
  rdata += dns_name;

  record.SetOwnedRdata(std::move(rdata));

  return record;
}



DnsResourceRecord BuildIntegrityRecord(
    std::string name,
    const std::vector<uint8_t>& serialized_rdata) {
  CHECK(!name.empty());

  DnsResourceRecord record;
  record.name = std::move(name);
  record.type = dns_protocol::kExperimentalTypeIntegrity;
  record.klass = dns_protocol::kClassIN;
  record.ttl = base::TimeDelta::FromDays(1).InSeconds();

  std::string serialized_rdata_str(serialized_rdata.begin(),
                                   serialized_rdata.end());
  record.SetOwnedRdata(std::move(serialized_rdata_str));

  CHECK_EQ(record.rdata.data(), record.owned_rdata.data());

  return record;
}

}  // namespace

DnsResourceRecord BuildTestAddressRecord(std::string name,
                                         const IPAddress& ip) {
  DCHECK(!name.empty());
  DCHECK(ip.IsValid());

  DnsResourceRecord record;
  record.name = std::move(name);
  record.type = ip.IsIPv4() ? dns_protocol::kTypeA : dns_protocol::kTypeAAAA;
  record.klass = dns_protocol::kClassIN;
  record.ttl = base::TimeDelta::FromDays(1).InSeconds();
  record.SetOwnedRdata(net::IPAddressToPackedString(ip));

  return record;
}


std::unique_ptr<DnsResponse> BuildTestDnsResponse(std::string name,
                                                  const IPAddress& ip) {
  DCHECK(ip.IsValid());

  std::vector<DnsResourceRecord> answers = {BuildTestAddressRecord(name, ip)};
  std::string dns_name;
  CHECK(DNSDomainFromDot(name, &dns_name));
  base::Optional<DnsQuery> query(
      base::in_place, 0, dns_name,
      ip.IsIPv4() ? dns_protocol::kTypeA : dns_protocol::kTypeAAAA);
  return std::make_unique<DnsResponse>(
      0, false, std::move(answers),
      std::vector<DnsResourceRecord>() /* authority_records */,
      std::vector<DnsResourceRecord>() /* additional_records */, query);
}

std::unique_ptr<DnsResponse> BuildTestDnsResponseWithCname(
    std::string name,
    const IPAddress& ip,
    std::string cannonname) {
  DCHECK(ip.IsValid());
  DCHECK(!cannonname.empty());

  std::vector<DnsResourceRecord> answers = {
      BuildCannonnameRecord(name, cannonname),
      BuildTestAddressRecord(cannonname, ip)};
  std::string dns_name;
  CHECK(DNSDomainFromDot(name, &dns_name));
  base::Optional<DnsQuery> query(
      base::in_place, 0, dns_name,
      ip.IsIPv4() ? dns_protocol::kTypeA : dns_protocol::kTypeAAAA);
  return std::make_unique<DnsResponse>(
      0, false, std::move(answers),
      std::vector<DnsResourceRecord>() /* authority_records */,
      std::vector<DnsResourceRecord>() /* additional_records */, query);
}

std::unique_ptr<DnsResponse> BuildTestDnsTextResponse(
    std::string name,
    std::vector<std::vector<std::string>> text_records,
    std::string answer_name) {
  if (answer_name.empty())
    answer_name = name;

  std::vector<DnsResourceRecord> answers;
  for (std::vector<std::string>& text_record : text_records) {
    answers.push_back(BuildTextRecord(answer_name, std::move(text_record)));
  }

  std::string dns_name;
  CHECK(DNSDomainFromDot(name, &dns_name));
  base::Optional<DnsQuery> query(base::in_place, 0, dns_name,
                                 dns_protocol::kTypeTXT);

  return std::make_unique<DnsResponse>(
      0, false, std::move(answers),
      std::vector<DnsResourceRecord>() /* authority_records */,
      std::vector<DnsResourceRecord>() /* additional_records */, query);
}

std::unique_ptr<DnsResponse> BuildTestDnsPointerResponse(
    std::string name,
    std::vector<std::string> pointer_names,
    std::string answer_name) {
  if (answer_name.empty())
    answer_name = name;

  std::vector<DnsResourceRecord> answers;
  for (std::string& pointer_name : pointer_names) {
    answers.push_back(BuildPointerRecord(answer_name, std::move(pointer_name)));
  }

  std::string dns_name;
  CHECK(DNSDomainFromDot(name, &dns_name));
  base::Optional<DnsQuery> query(base::in_place, 0, dns_name,
                                 dns_protocol::kTypePTR);

  return std::make_unique<DnsResponse>(
      0, false, std::move(answers),
      std::vector<DnsResourceRecord>() /* authority_records */,
      std::vector<DnsResourceRecord>() /* additional_records */, query);
}

std::unique_ptr<DnsResponse> BuildTestDnsServiceResponse(
    std::string name,
    std::vector<TestServiceRecord> service_records,
    std::string answer_name) {
  if (answer_name.empty())
    answer_name = name;

  std::vector<DnsResourceRecord> answers;
  for (TestServiceRecord& service_record : service_records) {
    answers.push_back(
        BuildServiceRecord(answer_name, std::move(service_record)));
  }

  std::string dns_name;
  CHECK(DNSDomainFromDot(name, &dns_name));
  base::Optional<DnsQuery> query(base::in_place, 0, dns_name,
                                 dns_protocol::kTypeSRV);

  return std::make_unique<DnsResponse>(
      0, false, std::move(answers),
      std::vector<DnsResourceRecord>() /* authority_records */,
      std::vector<DnsResourceRecord>() /* additional_records */, query);
}

std::unique_ptr<DnsResponse> BuildTestDnsIntegrityResponse(
    std::string hostname,
    const std::vector<uint8_t>& serialized_rdata) {
  CHECK(!hostname.empty());

  std::vector<DnsResourceRecord> answers{
      BuildIntegrityRecord(hostname, serialized_rdata)};

  std::string dns_name;
  CHECK(DNSDomainFromDot(hostname, &dns_name));
  base::Optional<DnsQuery> query(base::in_place, 0, dns_name,
                                 dns_protocol::kExperimentalTypeIntegrity);

  return std::make_unique<DnsResponse>(
      0, false, std::move(answers),
      std::vector<DnsResourceRecord>() /*  authority_records  */,
      std::vector<DnsResourceRecord>() /*  additional_records */, query);
}

MockDnsClientRule::Result::Result(ResultType type) : type(type) {}

MockDnsClientRule::Result::Result(std::unique_ptr<DnsResponse> response)
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
              authority_records = {BuildSoaRecord(hostname_)};
              result_.response = std::make_unique<DnsResponse>(
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
            case MockDnsClientRule::OK:
              if (result->response) {
                // Copy response in case |rules| are destroyed before the
                // transaction completes.
                result_.response = std::make_unique<DnsResponse>(
                    result->response->io_buffer(),
                    result->response->io_buffer_size());
                CHECK(result_.response->InitParseWithoutQuery(
                    result->response->io_buffer_size()));
              } else {
                // Generated response only available for address types.
                DCHECK(qtype_ == dns_protocol::kTypeA ||
                       qtype_ == dns_protocol::kTypeAAAA);
                result_.response = BuildTestDnsResponse(
                    hostname_, qtype_ == dns_protocol::kTypeA
                                   ? IPAddress::IPv4Localhost()
                                   : IPAddress::IPv6Localhost());
              }
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
  void Finish() {
    switch (result_.type) {
      case MockDnsClientRule::NODOMAIN:
      case MockDnsClientRule::FAIL:
        std::move(callback_).Run(this, ERR_NAME_NOT_RESOLVED,
                                 result_.response.get(), base::nullopt);
        break;
      case MockDnsClientRule::EMPTY:
      case MockDnsClientRule::OK:
      case MockDnsClientRule::MALFORMED:
        std::move(callback_).Run(this, OK, result_.response.get(),
                                 base::nullopt);
        break;
      case MockDnsClientRule::TIMEOUT:
        std::move(callback_).Run(this, ERR_DNS_TIMED_OUT, nullptr,
                                 base::nullopt);
        break;
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
    ResolveContext* resolve_context) {
  std::unique_ptr<MockTransaction> transaction =
      std::make_unique<MockTransaction>(
          rules_, hostname, qtype, secure, force_doh_server_available_,
          secure_dns_mode, resolve_context, std::move(callback));
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
