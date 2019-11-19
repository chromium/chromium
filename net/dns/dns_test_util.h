// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_TEST_UTIL_H_
#define NET_DNS_DNS_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"

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
static const int kT0TTL = 0x000000e4;
// +1 for the CNAME record.
static const unsigned kT0RecordCount = base::size(kT0IpAddresses) + 1;

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
static const int kT1TTL = 0x0000010b;
// +1 for the CNAME record.
static const unsigned kT1RecordCount = base::size(kT1IpAddresses) + 1;

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
static const int kT2TTL = 0x0000012c;
// +1 for the CNAME record.
static const unsigned kT2RecordCount = base::size(kT2IpAddresses) + 1;

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
static const int kT3TTL = 0x00000015;
// +2 for the CNAME records, +1 for TXT record.
static const unsigned kT3RecordCount = base::size(kT3IpAddresses) + 3;

//-----------------------------------------------------------------------------
// Query/response set for www.gstatic.com, ID is fixed to 4.
static const char kT4HostName[] = "www.gstatic.com";
static const uint16_t kT4Qtype = dns_protocol::kTypeA;
static const char kT4DnsName[] = {0x03, 'w', 'w', 'w', 0x07, 'g',
                                  's',  't', 'a', 't', 'i',  'c',
                                  0x03, 'c', 'o', 'm', 0x00};
static const size_t kT4QuerySize = 33;
static const uint8_t kT4ResponseDatagram[] = {
    // response contains the following IP addresses: 172.217.6.195.
    0x00, 0x04, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x77, 0x77, 0x77, 0x07, 0x67, 0x73, 0x74,
    0x61, 0x74, 0x69, 0x63, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00,
    0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x01, 0x2b, 0x00, 0x04, 0xac, 0xd9, 0x06, 0xc3};

static const char* const kT4IpAddresses[] = {"172.217.6.195"};
static const int kT4TTL = 0x0000012b;
static const unsigned kT4RecordCount = base::size(kT0IpAddresses);

//--------------------------------------------------------------------
// A well-formed ESNI (TLS 1.3 Encrypted Server Name Indication,
// draft 4) keys object ("ESNIKeys" member of the ESNIRecord struct from
// the spec).
//
// (This is cribbed from boringssl SSLTest.ESNIKeysDeserialize (CL 37704/13).)
extern const char kWellFormedEsniKeys[];
extern const size_t kWellFormedEsniKeysSize;

// Returns a well-formed ESNI keys object identical to kWellFormedEsniKeys,
// except that the first 0x22 bytes of |custom_data| are written over
// fields of the keys object in a manner that leaves length prefixes
// correct and enum members valid, and so that distinct values of
// |custom_data| result in distinct returned keys.
std::string GenerateWellFormedEsniKeys(base::StringPiece custom_data = "");

class AddressSorter;
class DnsClient;
class IPAddress;
class URLRequestContext;

// Builds an address record for the given name and IP.
DnsResourceRecord BuildTestAddressRecord(std::string name, const IPAddress& ip);

// Builds a DNS response that includes address records.
std::unique_ptr<DnsResponse> BuildTestDnsResponse(std::string name,
                                                  const IPAddress& ip);
std::unique_ptr<DnsResponse> BuildTestDnsResponseWithCname(
    std::string name,
    const IPAddress& ip,
    std::string cannonname);

// If |answer_name| is empty, |name| will be used for all answer records, as is
// the normal behavior.
std::unique_ptr<DnsResponse> BuildTestDnsTextResponse(
    std::string name,
    std::vector<std::vector<std::string>> text_records,
    std::string answer_name = "");
std::unique_ptr<DnsResponse> BuildTestDnsPointerResponse(
    std::string name,
    std::vector<std::string> pointer_names,
    std::string answer_name = "");

struct TestServiceRecord {
  uint16_t priority;
  uint16_t weight;
  uint16_t port;
  std::string target;
};

std::unique_ptr<DnsResponse> BuildTestDnsServiceResponse(
    std::string name,
    std::vector<TestServiceRecord> service_records,
    std::string answer_name = "");

std::unique_ptr<DnsResponse> BuildTestDnsEsniResponse(
    std::string hostname,
    std::vector<EsniContent> esni_records,
    std::string answer_name = "");

struct MockDnsClientRule {
  enum ResultType {
    NODOMAIN,   // Fail asynchronously with ERR_NAME_NOT_RESOLVED and NXDOMAIN.
    FAIL,       // Fail asynchronously with ERR_NAME_NOT_RESOLVED.
    TIMEOUT,    // Fail asynchronously with ERR_DNS_TIMED_OUT.
    EMPTY,      // Return an empty response.
    MALFORMED,  // "Succeed" but with an unparsable response.

    // Results in the response in |Result::response| or, if null, results in a
    // localhost IP response.
    OK,
  };

  struct Result {
    explicit Result(ResultType type);
    explicit Result(std::unique_ptr<DnsResponse> response);
    Result(Result&& result);
    ~Result();

    Result& operator=(Result&& result);

    ResultType type;
    std::unique_ptr<DnsResponse> response;
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
  URLRequestContext* context;
};

typedef std::vector<MockDnsClientRule> MockDnsClientRuleList;

// A DnsTransactionFactory which creates MockTransaction.
class MockDnsTransactionFactory : public DnsTransactionFactory {
 public:
  explicit MockDnsTransactionFactory(MockDnsClientRuleList rules);
  ~MockDnsTransactionFactory() override;

  std::unique_ptr<DnsTransaction> CreateTransaction(
      const std::string& hostname,
      uint16_t qtype,
      DnsTransactionFactory::CallbackType callback,
      const NetLogWithSource&,
      bool secure,
      DnsConfig::SecureDnsMode secure_dns_mode,
      URLRequestContext* url_request_context) override;

  void AddEDNSOption(const OptRecordRdata::Opt& opt) override;

  base::TimeDelta GetDelayUntilNextProbeForTest(
      unsigned doh_server_index) override;

  void StartDohProbes(URLRequestContext* url_request_context,
                      bool network_change) override;

  void CancelDohProbes() override;

  DnsConfig::SecureDnsMode GetSecureDnsModeForTest() override;

  void CompleteDelayedTransactions();
  // If there are any pending transactions of the given type,
  // completes one and returns true. Otherwise, returns false.
  bool CompleteOneDelayedTransactionOfType(DnsQueryType type)
      WARN_UNUSED_RESULT;

  bool doh_probes_running() { return doh_probes_running_; }

 private:
  class MockTransaction;
  using DelayedTransactionList = std::vector<base::WeakPtr<MockTransaction>>;

  MockDnsClientRuleList rules_;
  DelayedTransactionList delayed_transactions_;
  bool doh_probes_running_ = false;
};

// MockDnsClient provides MockDnsTransactionFactory.
class MockDnsClient : public DnsClient {
 public:
  MockDnsClient(DnsConfig config, MockDnsClientRuleList rules);
  ~MockDnsClient() override;

  // DnsClient interface:
  bool CanUseSecureDnsTransactions() const override;
  bool CanUseInsecureDnsTransactions() const override;
  void SetInsecureEnabled(bool enabled) override;
  bool FallbackFromSecureTransactionPreferred() const override;
  bool FallbackFromInsecureTransactionPreferred() const override;
  bool SetSystemConfig(base::Optional<DnsConfig> system_config) override;
  bool SetConfigOverrides(DnsConfigOverrides config_overrides) override;
  const DnsConfig* GetEffectiveConfig() const override;
  const DnsHosts* GetHosts() const override;
  void ActivateDohProbes(URLRequestContext* url_request_context) override;
  void CancelDohProbes() override;
  DnsTransactionFactory* GetTransactionFactory() override;
  AddressSorter* GetAddressSorter() override;
  void IncrementInsecureFallbackFailures() override;
  void ClearInsecureFallbackFailures() override;
  base::Optional<DnsConfig> GetSystemConfigForTesting() const override;
  DnsConfigOverrides GetConfigOverridesForTesting() const override;
  void SetProbeSuccessForTest(unsigned index, bool success) override;
  void SetTransactionFactoryForTesting(
      std::unique_ptr<DnsTransactionFactory> factory) override;

  // Completes all DnsTransactions that were delayed by a rule.
  void CompleteDelayedTransactions();
  // If there are any pending transactions of the given type,
  // completes one and returns true. Otherwise, returns false.
  bool CompleteOneDelayedTransactionOfType(DnsQueryType type)
      WARN_UNUSED_RESULT;

  void set_max_fallback_failures(int max_fallback_failures) {
    max_fallback_failures_ = max_fallback_failures;
  }

  void set_ignore_system_config_changes(bool ignore_system_config_changes) {
    ignore_system_config_changes_ = ignore_system_config_changes;
  }

  void set_doh_server_available(bool available) {
    doh_server_available_ = available;
  }

  MockDnsTransactionFactory* factory() { return factory_.get(); }

 private:
  base::Optional<DnsConfig> BuildEffectiveConfig();

  bool insecure_enabled_ = false;
  int fallback_failures_ = 0;
  int max_fallback_failures_ = DnsClient::kMaxInsecureFallbackFailures;
  bool ignore_system_config_changes_ = false;
  bool doh_server_available_ = true;
  URLRequestContext* probe_context_ = nullptr;

  base::Optional<DnsConfig> config_;
  DnsConfigOverrides overrides_;
  base::Optional<DnsConfig> effective_config_;
  std::unique_ptr<MockDnsTransactionFactory> factory_;
  std::unique_ptr<AddressSorter> address_sorter_;
};

}  // namespace net

#endif  // NET_DNS_DNS_TEST_UTIL_H_
