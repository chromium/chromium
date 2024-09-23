// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_TEST_UTIL_H_
#define NET_DNS_DNS_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/socket/socket_test_util.h"
#include "url/scheme_host_port.h"

namespace net {

//-----------------------------------------------------------------------------
// Query/response set for www.google.com, ID is fixed to 0.
static const char kT0HostName[] = "www.google.com";
static const uint16_t kT0Qtype = dns_protocol::kTypeA;
static const char kT0DnsName[] = {
  0x03, 'w', 'w', 'w',
  0x06, 'g', 'o', 'o', 'g', 'l', 'e',
  0x03, 'c', 'o', 'm',
  0x00
};
static const size_t kT0QuerySize = 32;
static const uint8_t kT0ResponseDatagram[] = {
    // response contains one CNAME for www.l.google.com and the following
    // IP addresses: 74.125.226.{179,180,176,177,178}
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x77, 0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
    0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x05,
    0x00, 0x01, 0x00, 0x01, 0x4d, 0x13, 0x00, 0x08, 0x03, 0x77, 0x77, 0x77,
    0x01, 0x6c, 0xc0, 0x10, 0xc0, 0x2c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0xe4, 0x00, 0x04, 0x4a, 0x7d, 0xe2, 0xb3, 0xc0, 0x2c, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x04, 0x4a, 0x7d, 0xe2, 0xb4,
    0xc0, 0x2c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x04,
    0x4a, 0x7d, 0xe2, 0xb0, 0xc0, 0x2c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0xe4, 0x00, 0x04, 0x4a, 0x7d, 0xe2, 0xb1, 0xc0, 0x2c, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x04, 0x4a, 0x7d, 0xe2, 0xb2};
static const char* const kT0IpAddresses[] = {
  "74.125.226.179", "74.125.226.180", "74.125.226.176",
  "74.125.226.177", "74.125.226.178"
};
static const char kT0CanonName[] = "www.l.google.com";
static const base::TimeDelta kT0Ttl = base::Seconds(0x000000e4);
// +1 for the CNAME record.
static const unsigned kT0RecordCount = std::size(kT0IpAddresses) + 1;

//-----------------------------------------------------------------------------
// Query/response set for codereview.chromium.org, ID is fixed to 1.
static const char kT1HostName[] = "codereview.chromium.org";
static const uint16_t kT1Qtype = dns_protocol::kTypeA;
static const char kT1DnsName[] = {
  0x0a, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w',
  0x08, 'c', 'h', 'r', 'o', 'm', 'i', 'u', 'm',
  0x03, 'o', 'r', 'g',
  0x00
};
static const size_t kT1QuerySize = 41;
static const uint8_t kT1ResponseDatagram[] = {
    // response contains one CNAME for ghs.l.google.com and the following
    // IP address: 64.233.169.121
    0x00, 0x01, 0x81, 0x80, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x0a, 0x63, 0x6f, 0x64, 0x65, 0x72, 0x65, 0x76, 0x69, 0x65,
    0x77, 0x08, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x69, 0x75, 0x6d, 0x03,
    0x6f, 0x72, 0x67, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00,
    0x05, 0x00, 0x01, 0x00, 0x01, 0x41, 0x75, 0x00, 0x12, 0x03, 0x67,
    0x68, 0x73, 0x01, 0x6c, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65,
    0x03, 0x63, 0x6f, 0x6d, 0x00, 0xc0, 0x35, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x00, 0x01, 0x0b, 0x00, 0x04, 0x40, 0xe9, 0xa9, 0x79};
static const char* const kT1IpAddresses[] = {
  "64.233.169.121"
};
static const char kT1CanonName[] = "ghs.l.google.com";
static const base::TimeDelta kT1Ttl = base::Seconds(0x0000010b);
// +1 for the CNAME record.
static const unsigned kT1RecordCount = std::size(kT1IpAddresses) + 1;

//-----------------------------------------------------------------------------
// Query/response set for www.ccs.neu.edu, ID is fixed to 2.
static const char kT2HostName[] = "www.ccs.neu.edu";
static const uint16_t kT2Qtype = dns_protocol::kTypeA;
static const char kT2DnsName[] = {
  0x03, 'w', 'w', 'w',
  0x03, 'c', 'c', 's',
  0x03, 'n', 'e', 'u',
  0x03, 'e', 'd', 'u',
  0x00
};
static const size_t kT2QuerySize = 33;
static const uint8_t kT2ResponseDatagram[] = {
    // response contains one CNAME for vulcan.ccs.neu.edu and the following
    // IP address: 129.10.116.81
    0x00, 0x02, 0x81, 0x80, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x77, 0x77, 0x77, 0x03, 0x63, 0x63, 0x73, 0x03, 0x6e, 0x65, 0x75,
    0x03, 0x65, 0x64, 0x75, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00,
    0x05, 0x00, 0x01, 0x00, 0x00, 0x01, 0x2c, 0x00, 0x09, 0x06, 0x76, 0x75,
    0x6c, 0x63, 0x61, 0x6e, 0xc0, 0x10, 0xc0, 0x2d, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x00, 0x01, 0x2c, 0x00, 0x04, 0x81, 0x0a, 0x74, 0x51};
static const char* const kT2IpAddresses[] = {
  "129.10.116.81"
};
static const char kT2CanonName[] = "vulcan.ccs.neu.edu";
static const base::TimeDelta kT2Ttl = base::Seconds(0x0000012c);
// +1 for the CNAME record.
static const unsigned kT2RecordCount = std::size(kT2IpAddresses) + 1;

//-----------------------------------------------------------------------------
// Query/response set for www.google.az, ID is fixed to 3.
static const char kT3HostName[] = "www.google.az";
static const uint16_t kT3Qtype = dns_protocol::kTypeA;
static const char kT3DnsName[] = {
  0x03, 'w', 'w', 'w',
  0x06, 'g', 'o', 'o', 'g', 'l', 'e',
  0x02, 'a', 'z',
  0x00
};
static const size_t kT3QuerySize = 31;
static const uint8_t kT3ResponseDatagram[] = {
    // response contains www.google.com as CNAME for www.google.az and
    // www.l.google.com as CNAME for www.google.com and the following
    // IP addresses: 74.125.226.{178,179,180,176,177}
    // The TTLs on the records are: 0x00015099, 0x00025099, 0x00000415,
    // 0x00003015, 0x00002015, 0x00000015, 0x00001015.
    // The last record is an imaginary TXT record for t.google.com.
    0x00, 0x03, 0x81, 0x80, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x77, 0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x02,
    0x61, 0x7a, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x05, 0x00,
    0x01, 0x00, 0x01, 0x50, 0x99, 0x00, 0x10, 0x03, 0x77, 0x77, 0x77, 0x06,
    0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0xc0,
    0x2b, 0x00, 0x05, 0x00, 0x01, 0x00, 0x02, 0x50, 0x99, 0x00, 0x08, 0x03,
    0x77, 0x77, 0x77, 0x01, 0x6c, 0xc0, 0x2f, 0xc0, 0x47, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x04, 0x15, 0x00, 0x04, 0x4a, 0x7d, 0xe2, 0xb2, 0xc0,
    0x47, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x30, 0x15, 0x00, 0x04, 0x4a,
    0x7d, 0xe2, 0xb3, 0xc0, 0x47, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x20,
    0x15, 0x00, 0x04, 0x4a, 0x7d, 0xe2, 0xb4, 0xc0, 0x47, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x4a, 0x7d, 0xe2, 0xb0, 0xc0,
    0x47, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x10, 0x15, 0x00, 0x04, 0x4a,
    0x7d, 0xe2, 0xb1, 0x01, 0x74, 0xc0, 0x2f, 0x00, 0x10, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x04, 0xde, 0xad, 0xfe, 0xed};
static const char* const kT3IpAddresses[] = {
  "74.125.226.178", "74.125.226.179", "74.125.226.180",
  "74.125.226.176", "74.125.226.177"
};
static const char kT3CanonName[] = "www.l.google.com";
static const base::TimeDelta kT3Ttl = base::Seconds(0x00000015);
// +2 for the CNAME records, +1 for TXT record.
static const unsigned kT3RecordCount = std::size(kT3IpAddresses) + 3;

//-----------------------------------------------------------------------------
// Query/response set for www.gstatic.com, ID is fixed to 0.
static const char kT4HostName[] = "www.gstatic.com";
static const uint16_t kT4Qtype = dns_protocol::kTypeA;
static const char kT4DnsName[] = {0x03, 'w', 'w', 'w', 0x07, 'g',
                                  's',  't', 'a', 't', 'i',  'c',
                                  0x03, 'c', 'o', 'm', 0x00};
static const size_t kT4QuerySize = 33;
static const uint8_t kT4ResponseDatagram[] = {
    // response contains the following IP addresses: 172.217.6.195.
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x77, 0x77, 0x77, 0x07, 0x67, 0x73, 0x74,
    0x61, 0x74, 0x69, 0x63, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00,
    0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x01, 0x2b, 0x00, 0x04, 0xac, 0xd9, 0x06, 0xc3};

static const char* const kT4IpAddresses[] = {"172.217.6.195"};
static const base::TimeDelta kT4Ttl = base::Seconds(0x0000012b);
static const unsigned kT4RecordCount = std::size(kT0IpAddresses);

class AddressSorter;
class DnsClient;
class DnsSession;
class IPAddress;
class ResolveContext;
class URLRequestContext;

DnsConfig CreateValidDnsConfig();

DnsResourceRecord BuildTestDnsRecord(std::string name,
                                     uint16_t type,
                                     std::string rdata,
                                     base::TimeDelta ttl = base::Days(1));

DnsResourceRecord BuildTestCnameRecord(std::string name,
                                       std::string_view canonical_name,
                                       base::TimeDelta ttl = base::Days(1));

DnsResourceRecord BuildTestAddressRecord(std::string name,
                                         const IPAddress& ip,
                                         base::TimeDelta ttl = base::Days(1));

DnsResourceRecord BuildTestTextRecord(std::string name,
                                      std::vector<std::string> text_strings,
                                      base::TimeDelta ttl = base::Days(1));

DnsResourceRecord BuildTestHttpsAliasRecord(
    std::string name,
    std::string_view alias_name,
    base::TimeDelta ttl = base::Days(1));

std::pair<uint16_t, std::string> BuildTestHttpsServiceAlpnParam(
    const std::vector<std::string>& alpns);

std::pair<uint16_t, std::string> BuildTestHttpsServiceEchConfigParam(
    base::span<const uint8_t> ech_config_list);

std::pair<uint16_t, std::string> BuildTestHttpsServiceMandatoryParam(
    std::vector<uint16_t> param_key_list);

std::pair<uint16_t, std::string> BuildTestHttpsServicePortParam(uint16_t port);

// `params` is a mapping from service param keys to a string containing the
// encoded bytes of a service param value (without the value length prefix which
// this method will automatically add).
DnsResourceRecord BuildTestHttpsServiceRecord(
    std::string name,
    uint16_t priority,
    std::string_view service_name,
    const std::map<uint16_t, std::string>& params,
    base::TimeDelta ttl = base::Days(1));

DnsResponse BuildTestDnsResponse(
    std::string name,
    uint16_t type,
    const std::vector<DnsResourceRecord>& answers,
    const std::vector<DnsResourceRecord>& authority = {},
    const std::vector<DnsResourceRecord>& additional = {},
    uint8_t rcode = dns_protocol::kRcodeNOERROR);

DnsResponse BuildTestDnsAddressResponse(std::string name,
                                        const IPAddress& ip,
                                        std::string answer_name = "");
DnsResponse BuildTestDnsAddressResponseWithCname(std::string name,
                                                 const IPAddress& ip,
                                                 std::string cannonname,
                                                 std::string answer_name = "");

// If |answer_name| is empty, |name| will be used for all answer records, as is
// the normal behavior.
DnsResponse BuildTestDnsTextResponse(
    std::string name,
    std::vector<std::vector<std::string>> text_records,
    std::string answer_name = "");
DnsResponse BuildTestDnsPointerResponse(std::string name,
                                        std::vector<std::string> pointer_names,
                                        std::string answer_name = "");

struct TestServiceRecord {
  uint16_t priority;
  uint16_t weight;
  uint16_t port;
  std::string target;
};

DnsResponse BuildTestDnsServiceResponse(
    std::string name,
    std::vector<TestServiceRecord> service_records,
    std::string answer_name = "");

struct MockDnsClientRule {
  enum class ResultType {
    // Fail asynchronously with ERR_NAME_NOT_RESOLVED and NXDOMAIN.
    kNoDomain,
    // Fail asynchronously with `net_error` or (if nullopt)
    // ERR_NAME_NOT_RESOLVED and  `response` if not nullopt.
    kFail,
    // Fail asynchronously with ERR_DNS_TIMED_OUT.
    kTimeout,
    // Simulates a slow transaction that will complete only with a lenient
    // timeout. Fails asynchronously with ERR_DNS_TIMED_OUT only if the
    // transaction was created with |fast_timeout|. Otherwise completes
    // successfully as if the ResultType were |kOk|.
    kSlow,
    // Return an empty response.
    kEmpty,
    // "Succeed" but with an unparsable response.
    kMalformed,
    // Immediately records a test failure if queried. Used to catch unexpected
    // queries. Alternately, if combined with `MockDnsClientRule::delay`, fails
    // only if the query is allowed to complete without being cancelled.
    kUnexpected,

    // Results in the response in |Result::response| or, if null, results in a
    // localhost IP response.
    kOk,
  };

  struct Result {
    explicit Result(ResultType type,
                    std::optional<DnsResponse> response = std::nullopt,
                    std::optional<int> net_error = std::nullopt);
    explicit Result(DnsResponse response);
    Result(Result&&);
    Result& operator=(Result&&);
    ~Result();

    ResultType type;
    std::optional<DnsResponse> response;
    std::optional<int> net_error;
  };

  // If |delay| is true, matching transactions will be delayed until triggered
  // by the consumer. If |context| is non-null, it will only match transactions
  // with the same context.
  MockDnsClientRule(const std::string& prefix,
                    uint16_t qtype,
                    bool secure,
                    Result result,
                    bool delay,
                    URLRequestContext* context = nullptr);
  MockDnsClientRule(MockDnsClientRule&& rule);

  Result result;
  std::string prefix;
  uint16_t qtype;
  bool secure;
  bool delay;
  raw_ptr<URLRequestContext, DanglingUntriaged> context;
};

typedef std::vector<MockDnsClientRule> MockDnsClientRuleList;

// A DnsTransactionFactory which creates MockTransaction.
class MockDnsTransactionFactory : public DnsTransactionFactory {
 public:
  explicit MockDnsTransactionFactory(MockDnsClientRuleList rules);
  ~MockDnsTransactionFactory() override;

  std::unique_ptr<DnsTransaction> CreateTransaction(
      std::string hostname,
      uint16_t qtype,
      const NetLogWithSource&,
      bool secure,
      SecureDnsMode secure_dns_mode,
      ResolveContext* resolve_context,
      bool fast_timeout) override;

  std::unique_ptr<DnsProbeRunner> CreateDohProbeRunner(
      ResolveContext* resolve_context) override;

  void AddEDNSOption(std::unique_ptr<OptRecordRdata::Opt> opt) override;

  SecureDnsMode GetSecureDnsModeForTest() override;

  void CompleteDelayedTransactions();
  // If there are any pending transactions of the given type,
  // completes one and returns true. Otherwise, returns false.
  [[nodiscard]] bool CompleteOneDelayedTransactionOfType(DnsQueryType type);

  bool doh_probes_running() { return !running_doh_probe_runners_.empty(); }
  void CompleteDohProbeRuners() { running_doh_probe_runners_.clear(); }

  void set_force_doh_server_available(bool available) {
    force_doh_server_available_ = available;
  }

 private:
  class MockTransaction;
  class MockDohProbeRunner;
  using DelayedTransactionList = std::vector<base::WeakPtr<MockTransaction>>;

  MockDnsClientRuleList rules_;
  DelayedTransactionList delayed_transactions_;

  bool force_doh_server_available_ = true;
  std::set<raw_ptr<MockDohProbeRunner, SetExperimental>>
      running_doh_probe_runners_;

  base::WeakPtrFactory<MockDnsTransactionFactory> weak_ptr_factory_{this};
};

// MockDnsClient provides MockDnsTransactionFactory.
class MockDnsClient : public DnsClient {
 public:
  MockDnsClient(DnsConfig config, MockDnsClientRuleList rules);
  ~MockDnsClient() override;

  // DnsClient interface:
  bool CanUseSecureDnsTransactions() const override;
  bool CanUseInsecureDnsTransactions() const override;
  bool CanQueryAdditionalTypesViaInsecureDns() const override;
  void SetInsecureEnabled(bool enabled, bool additional_types_enabled) override;
  bool FallbackFromSecureTransactionPreferred(
      ResolveContext* resolve_context) const override;
  bool FallbackFromInsecureTransactionPreferred() const override;
  bool SetSystemConfig(std::optional<DnsConfig> system_config) override;
  bool SetConfigOverrides(DnsConfigOverrides config_overrides) override;
  void ReplaceCurrentSession() override;
  DnsSession* GetCurrentSession() override;
  const DnsConfig* GetEffectiveConfig() const override;
  const DnsHosts* GetHosts() const override;
  DnsTransactionFactory* GetTransactionFactory() override;
  AddressSorter* GetAddressSorter() override;
  void IncrementInsecureFallbackFailures() override;
  void ClearInsecureFallbackFailures() override;
  base::Value::Dict GetDnsConfigAsValueForNetLog() const override;
  std::optional<DnsConfig> GetSystemConfigForTesting() const override;
  DnsConfigOverrides GetConfigOverridesForTesting() const override;
  void SetTransactionFactoryForTesting(
      std::unique_ptr<DnsTransactionFactory> factory) override;
  void SetAddressSorterForTesting(
      std::unique_ptr<AddressSorter> address_sorter) override;
  std::optional<std::vector<IPEndPoint>> GetPresetAddrs(
      const url::SchemeHostPort& endpoint) const override;

  // Completes all DnsTransactions that were delayed by a rule.
  void CompleteDelayedTransactions();
  // If there are any pending transactions of the given type,
  // completes one and returns true. Otherwise, returns false.
  [[nodiscard]] bool CompleteOneDelayedTransactionOfType(DnsQueryType type);

  void set_max_fallback_failures(int max_fallback_failures) {
    max_fallback_failures_ = max_fallback_failures;
  }

  void set_ignore_system_config_changes(bool ignore_system_config_changes) {
    ignore_system_config_changes_ = ignore_system_config_changes;
  }

  void set_preset_endpoint(std::optional<url::SchemeHostPort> endpoint) {
    preset_endpoint_ = std::move(endpoint);
  }

  void set_preset_addrs(std::vector<IPEndPoint> preset_addrs) {
    preset_addrs_ = std::move(preset_addrs);
  }

  void SetForceDohServerAvailable(bool available);

  MockDnsTransactionFactory* factory() { return factory_.get(); }

 private:
  std::optional<DnsConfig> BuildEffectiveConfig();
  scoped_refptr<DnsSession> BuildSession();

  bool insecure_enabled_ = false;
  bool additional_types_enabled_ = false;
  int fallback_failures_ = 0;
  int max_fallback_failures_ = DnsClient::kMaxInsecureFallbackFailures;
  bool ignore_system_config_changes_ = false;

  // If |true|, MockDnsClient will always pretend DoH servers are available and
  // allow secure transactions no matter what the state is in the transaction
  // ResolveContext. If |false|, the ResolveContext must contain at least one
  // available DoH server to allow secure transactions.
  bool force_doh_server_available_ = true;

  MockClientSocketFactory socket_factory_;
  std::optional<DnsConfig> config_;
  scoped_refptr<DnsSession> session_;
  DnsConfigOverrides overrides_;
  std::optional<DnsConfig> effective_config_;
  std::unique_ptr<MockDnsTransactionFactory> factory_;
  std::unique_ptr<AddressSorter> address_sorter_;
  std::optional<url::SchemeHostPort> preset_endpoint_;
  std::optional<std::vector<IPEndPoint>> preset_addrs_;
};

// A HostResolverProc that pushes each host mapped into a list and allows
// waiting for a specific number of requests. Unlike RuleBasedHostResolverProc
// it never calls SystemHostResolverCall. By default resolves all hostnames to
// "127.0.0.1". After AddRule(), it resolves only names explicitly specified.
class MockHostResolverProc : public HostResolverProc {
 public:
  struct ResolveKey {
    ResolveKey(const std::string& hostname,
               AddressFamily address_family,
               HostResolverFlags flags)
        : hostname(hostname), address_family(address_family), flags(flags) {}
    bool operator<(const ResolveKey& other) const {
      return std::tie(address_family, hostname, flags) <
             std::tie(other.address_family, other.hostname, other.flags);
    }
    std::string hostname;
    AddressFamily address_family;
    HostResolverFlags flags;
  };

  typedef std::vector<ResolveKey> CaptureList;

  MockHostResolverProc();

  MockHostResolverProc(const MockHostResolverProc&) = delete;
  MockHostResolverProc& operator=(const MockHostResolverProc&) = delete;

  // Waits until `count` calls to `Resolve` are blocked. Returns false when
  // timed out.
  bool WaitFor(unsigned count);

  // Signals `count` waiting calls to `Resolve`. First come first served.
  void SignalMultiple(unsigned count);

  // Signals all waiting calls to `Resolve`. Beware of races.
  void SignalAll();

  void AddRule(const std::string& hostname,
               AddressFamily family,
               const AddressList& result,
               HostResolverFlags flags = 0);

  void AddRule(const std::string& hostname,
               AddressFamily family,
               const std::string& ip_list,
               HostResolverFlags flags = 0,
               const std::string& canonical_name = "");

  void AddRuleForAllFamilies(const std::string& hostname,
                             const std::string& ip_list,
                             HostResolverFlags flags = 0,
                             const std::string& canonical_name = "");

  int Resolve(const std::string& hostname,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override;

  CaptureList GetCaptureList() const;

  void ClearCaptureList();

  bool HasBlockedRequests() const;

 protected:
  ~MockHostResolverProc() override;

 private:
  mutable base::Lock lock_;
  std::map<ResolveKey, AddressList> rules_;
  CaptureList capture_list_;
  unsigned num_requests_waiting_ = 0;
  unsigned num_slots_available_ = 0;
  base::ConditionVariable requests_waiting_;
  base::ConditionVariable slots_available_;
};

}  // namespace net

#endif  // NET_DNS_DNS_TEST_UTIL_H_
