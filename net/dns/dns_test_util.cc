// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_test_util.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/sys_byteorder.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_session.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/resolve_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

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
  std::optional<std::vector<uint8_t>> dns_name =
      dns_names_util::DottedNameToNetwork(hostname);
  CHECK(dns_name.has_value());
  DnsQuery query(/*id=*/0x14, dns_name.value(), type);

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
  void Sort(const std::vector<IPEndPoint>& endpoints,
            CallbackType callback) const override {
    // Do nothing.
    std::move(callback).Run(true, endpoints);
  }
};

}  // namespace

DnsConfig CreateValidDnsConfig() {
  IPAddress dns_ip(192, 168, 1, 0);
  DnsConfig config;
  config.nameservers.emplace_back(dns_ip, dns_protocol::kDefaultPort);
  config.doh_config =
      *DnsOverHttpsConfig::FromString("https://dns.example.com/");
  config.secure_dns_mode = SecureDnsMode::kOff;
  EXPECT_TRUE(config.IsValid());
  return config;
}

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
                                       std::string_view canonical_name,
                                       base::TimeDelta ttl) {
  DCHECK(!name.empty());
  DCHECK(!canonical_name.empty());

  std::optional<std::vector<uint8_t>> rdata =
      dns_names_util::DottedNameToNetwork(canonical_name);
  CHECK(rdata.has_value());

  return BuildTestDnsRecord(
      std::move(name), dns_protocol::kTypeCNAME,
      std::string(reinterpret_cast<char*>(rdata.value().data()),
                  rdata.value().size()),
      ttl);
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
                                            std::string_view alias_name,
                                            base::TimeDelta ttl) {
  DCHECK(!name.empty());

  std::string rdata("\000\000", 2);

  std::optional<std::vector<uint8_t>> alias_domain =
      dns_names_util::DottedNameToNetwork(alias_name);
  CHECK(alias_domain.has_value());
  rdata.append(reinterpret_cast<char*>(alias_domain.value().data()),
               alias_domain.value().size());

  return BuildTestDnsRecord(std::move(name), dns_protocol::kTypeHttps,
                            std::move(rdata), ttl);
}

std::pair<uint16_t, std::string> BuildTestHttpsServiceAlpnParam(
    const std::vector<std::string>& alpns) {
  std::string param_value;

  for (const std::string& alpn : alpns) {
    CHECK(!alpn.empty());
    param_value.append(
        1, static_cast<char>(base::checked_cast<uint8_t>(alpn.size())));
    param_value.append(alpn);
  }

  return std::pair(dns_protocol::kHttpsServiceParamKeyAlpn,
                   std::move(param_value));
}

std::pair<uint16_t, std::string> BuildTestHttpsServiceEchConfigParam(
    base::span<const uint8_t> ech_config_list) {
  return std::pair(
      dns_protocol::kHttpsServiceParamKeyEchConfig,
      std::string(reinterpret_cast<const char*>(ech_config_list.data()),
                  ech_config_list.size()));
}

std::pair<uint16_t, std::string> BuildTestHttpsServiceMandatoryParam(
    std::vector<uint16_t> param_key_list) {
  base::ranges::sort(param_key_list);

  std::string value;
  for (uint16_t param_key : param_key_list) {
    std::array<uint8_t, 2> num_buffer = base::U16ToBigEndian(param_key);
    value.append(num_buffer.begin(), num_buffer.end());
  }

  return std::pair(dns_protocol::kHttpsServiceParamKeyMandatory,
                   std::move(value));
}

std::pair<uint16_t, std::string> BuildTestHttpsServicePortParam(uint16_t port) {
  std::array<uint8_t, 2> buffer = base::U16ToBigEndian(port);
  return std::pair(dns_protocol::kHttpsServiceParamKeyPort,
                   std::string(buffer.begin(), buffer.end()));
}

DnsResourceRecord BuildTestHttpsServiceRecord(
    std::string name,
    uint16_t priority,
    std::string_view service_name,
    const std::map<uint16_t, std::string>& params,
    base::TimeDelta ttl) {
  DCHECK(!name.empty());
  DCHECK_NE(priority, 0);

  std::string rdata;

  {
    std::array<uint8_t, 2> buf = base::U16ToBigEndian(priority);
    rdata.append(buf.begin(), buf.end());
  }

  std::optional<std::vector<uint8_t>> service_domain;
  if (service_name == ".") {
    // HTTPS records have special behavior for `service_name == "."` (that it
    // will be treated as if the service name is the same as the record owner
    // name), so allow such inputs despite normally being disallowed for
    // Chrome-encoded DNS names.
    service_domain = std::vector<uint8_t>{0};
  } else {
    service_domain = dns_names_util::DottedNameToNetwork(service_name);
  }
  CHECK(service_domain.has_value());
  rdata.append(reinterpret_cast<char*>(service_domain.value().data()),
               service_domain.value().size());

  for (auto& param : params) {
    {
      std::array<uint8_t, 2> buf = base::U16ToBigEndian(param.first);
      rdata.append(buf.begin(), buf.end());
    }
    {
      std::array<uint8_t, 2> buf = base::U16ToBigEndian(
          base::checked_cast<uint16_t>(param.second.size()));
      rdata.append(buf.begin(), buf.end());
    }
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

  std::optional<std::vector<uint8_t>> dns_name =
      dns_names_util::DottedNameToNetwork(name);
  CHECK(dns_name.has_value());

  std::optional<DnsQuery> query(std::in_place, 0, dns_name.value(), type);
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

  std::optional<std::vector<uint8_t>> cname_rdata =
      dns_names_util::DottedNameToNetwork(cannonname);
  CHECK(cname_rdata.has_value());

  std::vector<DnsResourceRecord> answers = {
      BuildTestDnsRecord(
          std::move(answer_name), dns_protocol::kTypeCNAME,
          std::string(reinterpret_cast<char*>(cname_rdata.value().data()),
                      cname_rdata.value().size())),
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
    std::optional<std::vector<uint8_t>> rdata =
        dns_names_util::DottedNameToNetwork(pointer_name);
    CHECK(rdata.has_value());

    answers.push_back(BuildTestDnsRecord(
        answer_name, dns_protocol::kTypePTR,
        std::string(reinterpret_cast<char*>(rdata.value().data()),
                    rdata.value().size())));
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
    {
      std::array<uint8_t, 2> buf =
          base::U16ToBigEndian(service_record.priority);
      rdata.append(buf.begin(), buf.end());
    }
    {
      std::array<uint8_t, 2> buf = base::U16ToBigEndian(service_record.weight);
      rdata.append(buf.begin(), buf.end());
    }
    {
      std::array<uint8_t, 2> buf = base::U16ToBigEndian(service_record.port);
      rdata.append(buf.begin(), buf.end());
    }

    std::optional<std::vector<uint8_t>> dns_name =
        dns_names_util::DottedNameToNetwork(service_record.target);
    CHECK(dns_name.has_value());
    rdata.append(reinterpret_cast<char*>(dns_name.value().data()),
                 dns_name.value().size());

    answers.push_back(BuildTestDnsRecord(answer_name, dns_protocol::kTypeSRV,
                                         std::move(rdata), base::Hours(5)));
  }

  return BuildTestDnsResponse(std::move(name), dns_protocol::kTypeSRV, answers);
}

MockDnsClientRule::Result::Result(ResultType type,
                                  std::optional<DnsResponse> response,
                                  std::optional<int> net_error)
    : type(type), response(std::move(response)), net_error(net_error) {}

MockDnsClientRule::Result::Result(DnsResponse response)
    : type(ResultType::kOk),
      response(std::move(response)),
      net_error(std::nullopt) {}

MockDnsClientRule::Result::Result(Result&&) = default;

MockDnsClientRule::Result& MockDnsClientRule::Result::operator=(Result&&) =
    default;

MockDnsClientRule::Result::~Result() = default;

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
class MockDnsTransactionFactory::MockTransaction final : public DnsTransaction {
 public:
  MockTransaction(const MockDnsClientRuleList& rules,
                  std::string hostname,
                  uint16_t qtype,
                  bool secure,
                  bool force_doh_server_available,
                  SecureDnsMode secure_dns_mode,
                  ResolveContext* resolve_context,
                  bool fast_timeout)
      : hostname_(std::move(hostname)), qtype_(qtype) {
    // Do not allow matching any rules if transaction is secure and no DoH
    // servers are available.
    if (!secure || force_doh_server_available ||
        resolve_context->NumAvailableDohServers(
            resolve_context->current_session_for_testing()) > 0) {
      // Find the relevant rule which matches |qtype|, |secure|, prefix of
      // |hostname_|, and |url_request_context| (iff the rule context is not
      // null).
      for (const auto& rule : rules) {
        const std::string& prefix = rule.prefix;
        if ((rule.qtype == qtype) && (rule.secure == secure) &&
            (hostname_.size() >= prefix.size()) &&
            (hostname_.compare(0, prefix.size(), prefix) == 0) &&
            (!rule.context ||
             rule.context == resolve_context->url_request_context())) {
          const MockDnsClientRule::Result* result = &rule.result;
          result_ = MockDnsClientRule::Result(result->type);
          result_.net_error = result->net_error;
          delayed_ = rule.delay;

          // Generate a DnsResponse when not provided with the rule.
          std::vector<DnsResourceRecord> authority_records;
          std::optional<std::vector<uint8_t>> dns_name =
              dns_names_util::DottedNameToNetwork(hostname_);
          CHECK(dns_name.has_value());
          std::optional<DnsQuery> query(std::in_place, /*id=*/22,
                                        dns_name.value(), qtype_);
          switch (result->type) {
            case MockDnsClientRule::ResultType::kNoDomain:
            case MockDnsClientRule::ResultType::kEmpty:
              DCHECK(!result->response);  // Not expected to be provided.
              authority_records = {BuildTestDnsRecord(
                  hostname_, dns_protocol::kTypeSOA, "fake rdata")};
              result_.response = DnsResponse(
                  22 /* id */, false /* is_authoritative */,
                  std::vector<DnsResourceRecord>() /* answers */,
                  authority_records,
                  std::vector<DnsResourceRecord>() /* additional_records */,
                  query,
                  result->type == MockDnsClientRule::ResultType::kNoDomain
                      ? dns_protocol::kRcodeNXDOMAIN
                      : 0);
              break;
            case MockDnsClientRule::ResultType::kFail:
              if (result->response)
                SetResponse(result);
              break;
            case MockDnsClientRule::ResultType::kTimeout:
              DCHECK(!result->response);  // Not expected to be provided.
              break;
            case MockDnsClientRule::ResultType::kSlow:
              if (!fast_timeout)
                SetResponse(result);
              break;
            case MockDnsClientRule::ResultType::kOk:
              SetResponse(result);
              break;
            case MockDnsClientRule::ResultType::kMalformed:
              DCHECK(!result->response);  // Not expected to be provided.
              result_.response = CreateMalformedResponse(hostname_, qtype_);
              break;
            case MockDnsClientRule::ResultType::kUnexpected:
              if (!delayed_) {
                // Assume a delayed kUnexpected transaction is only an issue if
                // allowed to complete.
                ADD_FAILURE()
                    << "Unexpected DNS transaction created for hostname "
                    << hostname_;
              }
              break;
          }

          break;
        }
      }
    }
  }

  const std::string& GetHostname() const override { return hostname_; }

  uint16_t GetType() const override { return qtype_; }

  void Start(ResponseCallback callback) override {
    CHECK(!callback.is_null());
    CHECK(callback_.is_null());
    EXPECT_FALSE(started_);

    callback_ = std::move(callback);
    started_ = true;
    if (delayed_)
      return;
    // Using WeakPtr to cleanly cancel when transaction is destroyed.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MockTransaction::Finish,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void FinishDelayedTransaction() {
    EXPECT_TRUE(delayed_);
    delayed_ = false;
    Finish();
  }

  bool delayed() const { return delayed_; }

  base::WeakPtr<MockTransaction> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void SetResponse(const MockDnsClientRule::Result* result) {
    if (result->response) {
      // Copy response in case |result| is destroyed before the transaction
      // completes.
      auto buffer_copy = base::MakeRefCounted<IOBufferWithSize>(
          result->response->io_buffer_size());
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
      case MockDnsClientRule::ResultType::kNoDomain:
      case MockDnsClientRule::ResultType::kFail: {
        int error = result_.net_error.value_or(ERR_NAME_NOT_RESOLVED);
        DCHECK_NE(error, OK);
        std::move(callback_).Run(error, base::OptionalToPtr(result_.response));
        break;
      }
      case MockDnsClientRule::ResultType::kEmpty:
      case MockDnsClientRule::ResultType::kOk:
      case MockDnsClientRule::ResultType::kMalformed:
        DCHECK(!result_.net_error.has_value());
        std::move(callback_).Run(OK, base::OptionalToPtr(result_.response));
        break;
      case MockDnsClientRule::ResultType::kTimeout:
        DCHECK(!result_.net_error.has_value());
        std::move(callback_).Run(ERR_DNS_TIMED_OUT, /*response=*/nullptr);
        break;
      case MockDnsClientRule::ResultType::kSlow:
        if (result_.response) {
          std::move(callback_).Run(
              result_.net_error.value_or(OK),
              result_.response ? &result_.response.value() : nullptr);
        } else {
          DCHECK(!result_.net_error.has_value());
          std::move(callback_).Run(ERR_DNS_TIMED_OUT, /*response=*/nullptr);
        }
        break;
      case MockDnsClientRule::ResultType::kUnexpected:
        ADD_FAILURE() << "Unexpected DNS transaction completed for hostname "
                      << hostname_;
        break;
    }
  }

  void SetRequestPriority(RequestPriority priority) override {}

  MockDnsClientRule::Result result_{MockDnsClientRule::ResultType::kFail};
  const std::string hostname_;
  const uint16_t qtype_;
  ResponseCallback callback_;
  bool started_ = false;
  bool delayed_ = false;
  base::WeakPtrFactory<MockTransaction> weak_ptr_factory_{this};
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
    NOTREACHED_IN_MIGRATION();
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
    std::string hostname,
    uint16_t qtype,
    const NetLogWithSource&,
    bool secure,
    SecureDnsMode secure_dns_mode,
    ResolveContext* resolve_context,
    bool fast_timeout) {
  std::unique_ptr<MockTransaction> transaction =
      std::make_unique<MockTransaction>(rules_, std::move(hostname), qtype,
                                        secure, force_doh_server_available_,
                                        secure_dns_mode, resolve_context,
                                        fast_timeout);
  if (transaction->delayed())
    delayed_transactions_.push_back(transaction->AsWeakPtr());
  return transaction;
}

std::unique_ptr<DnsProbeRunner> MockDnsTransactionFactory::CreateDohProbeRunner(
    ResolveContext* resolve_context) {
  return std::make_unique<MockDohProbeRunner>(weak_ptr_factory_.GetWeakPtr());
}

void MockDnsTransactionFactory::AddEDNSOption(
    std::unique_ptr<OptRecordRdata::Opt> opt) {}

SecureDnsMode MockDnsTransactionFactory::GetSecureDnsModeForTest() {
  return SecureDnsMode::kAutomatic;
}

void MockDnsTransactionFactory::CompleteDelayedTransactions() {
  DelayedTransactionList old_delayed_transactions;
  old_delayed_transactions.swap(delayed_transactions_);
  for (auto& old_delayed_transaction : old_delayed_transactions) {
    if (old_delayed_transaction.get())
      old_delayed_transaction->FinishDelayedTransaction();
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
      factory_(std::make_unique<MockDnsTransactionFactory>(std::move(rules))),
      address_sorter_(std::make_unique<MockAddressSorter>()) {
  effective_config_ = BuildEffectiveConfig();
  session_ = BuildSession();
}

MockDnsClient::~MockDnsClient() = default;

bool MockDnsClient::CanUseSecureDnsTransactions() const {
  const DnsConfig* config = GetEffectiveConfig();
  return config && config->IsValid() && !config->doh_config.servers().empty();
}

bool MockDnsClient::CanUseInsecureDnsTransactions() const {
  const DnsConfig* config = GetEffectiveConfig();
  return config && config->IsValid() && insecure_enabled_ &&
         !config->dns_over_tls_active;
}

bool MockDnsClient::CanQueryAdditionalTypesViaInsecureDns() const {
  DCHECK(CanUseInsecureDnsTransactions());
  return additional_types_enabled_;
}

void MockDnsClient::SetInsecureEnabled(bool enabled,
                                       bool additional_types_enabled) {
  insecure_enabled_ = enabled;
  additional_types_enabled_ = additional_types_enabled;
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

bool MockDnsClient::SetSystemConfig(std::optional<DnsConfig> system_config) {
  if (ignore_system_config_changes_)
    return false;

  std::optional<DnsConfig> before = effective_config_;
  config_ = std::move(system_config);
  effective_config_ = BuildEffectiveConfig();
  session_ = BuildSession();
  return before != effective_config_;
}

bool MockDnsClient::SetConfigOverrides(DnsConfigOverrides config_overrides) {
  std::optional<DnsConfig> before = effective_config_;
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

base::Value::Dict MockDnsClient::GetDnsConfigAsValueForNetLog() const {
  // This is just a stub implementation that never produces a meaningful value.
  return base::Value::Dict();
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

std::optional<DnsConfig> MockDnsClient::GetSystemConfigForTesting() const {
  return config_;
}

DnsConfigOverrides MockDnsClient::GetConfigOverridesForTesting() const {
  return overrides_;
}

void MockDnsClient::SetTransactionFactoryForTesting(
    std::unique_ptr<DnsTransactionFactory> factory) {
  NOTREACHED_IN_MIGRATION();
}

void MockDnsClient::SetAddressSorterForTesting(
    std::unique_ptr<AddressSorter> address_sorter) {
  address_sorter_ = std::move(address_sorter);
}

std::optional<std::vector<IPEndPoint>> MockDnsClient::GetPresetAddrs(
    const url::SchemeHostPort& endpoint) const {
  EXPECT_THAT(preset_endpoint_, testing::Optional(endpoint));
  return preset_addrs_;
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

std::optional<DnsConfig> MockDnsClient::BuildEffectiveConfig() {
  if (overrides_.OverridesEverything())
    return overrides_.ApplyOverrides(DnsConfig());
  if (!config_ || !config_.value().IsValid())
    return std::nullopt;

  return overrides_.ApplyOverrides(config_.value());
}

scoped_refptr<DnsSession> MockDnsClient::BuildSession() {
  if (!effective_config_)
    return nullptr;

  // Session not expected to be used for anything that will actually require
  // random numbers.
  auto null_random_callback =
      base::BindRepeating([](int, int) -> int { base::ImmediateCrash(); });

  return base::MakeRefCounted<DnsSession>(
      effective_config_.value(), null_random_callback, nullptr /* net_log */);
}

MockHostResolverProc::MockHostResolverProc()
    : HostResolverProc(nullptr),
      requests_waiting_(&lock_),
      slots_available_(&lock_) {}

MockHostResolverProc::~MockHostResolverProc() = default;

bool MockHostResolverProc::WaitFor(unsigned count) {
  base::AutoLock lock(lock_);
  base::Time start_time = base::Time::Now();
  while (num_requests_waiting_ < count) {
    requests_waiting_.TimedWait(TestTimeouts::action_timeout());
    if (base::Time::Now() > start_time + TestTimeouts::action_timeout()) {
      return false;
    }
  }
  return true;
}

void MockHostResolverProc::SignalMultiple(unsigned count) {
  base::AutoLock lock(lock_);
  num_slots_available_ += count;
  slots_available_.Broadcast();
}

void MockHostResolverProc::SignalAll() {
  base::AutoLock lock(lock_);
  num_slots_available_ = num_requests_waiting_;
  slots_available_.Broadcast();
}

void MockHostResolverProc::AddRule(const std::string& hostname,
                                   AddressFamily family,
                                   const AddressList& result,
                                   HostResolverFlags flags) {
  base::AutoLock lock(lock_);
  rules_[ResolveKey(hostname, family, flags)] = result;
}

void MockHostResolverProc::AddRule(const std::string& hostname,
                                   AddressFamily family,
                                   const std::string& ip_list,
                                   HostResolverFlags flags,
                                   const std::string& canonical_name) {
  AddressList result;
  std::vector<std::string> dns_aliases;
  if (canonical_name != "") {
    dns_aliases = {canonical_name};
  }
  int rv = ParseAddressList(ip_list, &result.endpoints());
  result.SetDnsAliases(dns_aliases);
  DCHECK_EQ(OK, rv);
  AddRule(hostname, family, result, flags);
}

void MockHostResolverProc::AddRuleForAllFamilies(
    const std::string& hostname,
    const std::string& ip_list,
    HostResolverFlags flags,
    const std::string& canonical_name) {
  AddressList result;
  std::vector<std::string> dns_aliases;
  if (canonical_name != "") {
    dns_aliases = {canonical_name};
  }
  int rv = ParseAddressList(ip_list, &result.endpoints());
  result.SetDnsAliases(dns_aliases);
  DCHECK_EQ(OK, rv);
  AddRule(hostname, ADDRESS_FAMILY_UNSPECIFIED, result, flags);
  AddRule(hostname, ADDRESS_FAMILY_IPV4, result, flags);
  AddRule(hostname, ADDRESS_FAMILY_IPV6, result, flags);
}

int MockHostResolverProc::Resolve(const std::string& hostname,
                                  AddressFamily address_family,
                                  HostResolverFlags host_resolver_flags,
                                  AddressList* addrlist,
                                  int* os_error) {
  base::AutoLock lock(lock_);
  capture_list_.emplace_back(hostname, address_family, host_resolver_flags);
  ++num_requests_waiting_;
  requests_waiting_.Broadcast();
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting
        scoped_allow_base_sync_primitives;
    while (!num_slots_available_) {
      slots_available_.Wait();
    }
  }
  DCHECK_GT(num_requests_waiting_, 0u);
  --num_slots_available_;
  --num_requests_waiting_;
  if (rules_.empty()) {
    int rv = ParseAddressList("127.0.0.1", &addrlist->endpoints());
    DCHECK_EQ(OK, rv);
    return OK;
  }
  ResolveKey key(hostname, address_family, host_resolver_flags);
  if (rules_.count(key) == 0) {
    return ERR_NAME_NOT_RESOLVED;
  }
  *addrlist = rules_[key];
  return OK;
}

MockHostResolverProc::CaptureList MockHostResolverProc::GetCaptureList() const {
  CaptureList copy;
  {
    base::AutoLock lock(lock_);
    copy = capture_list_;
  }
  return copy;
}

void MockHostResolverProc::ClearCaptureList() {
  base::AutoLock lock(lock_);
  capture_list_.clear();
}

bool MockHostResolverProc::HasBlockedRequests() const {
  base::AutoLock lock(lock_);
  return num_requests_waiting_ > num_slots_available_;
}

}  // namespace net
