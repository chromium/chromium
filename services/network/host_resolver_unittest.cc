// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/host_resolver.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log.h"
#include "net/net_buildflags.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

namespace network {
namespace {

class HostResolverTest : public testing::Test {
 public:
  HostResolverTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
};

net::IPEndPoint CreateExpectedEndPoint(const std::string& address,
                                       uint16_t port) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(address));
  return net::IPEndPoint(ip_address, port);
}

class TestResolveHostClient : public mojom::ResolveHostClient {
 public:
  // If |run_loop| is non-null, will call RunLoop::Quit() on completion.
  TestResolveHostClient(mojo::PendingRemote<mojom::ResolveHostClient>* remote,
                        base::RunLoop* run_loop)
      : receiver_(this, remote->InitWithNewPipeAndPassReceiver()),
        complete_(false),
        top_level_result_error_(net::ERR_IO_PENDING),
        result_error_(net::ERR_UNEXPECTED),
        run_loop_(run_loop) {}

  void CloseReceiver() { receiver_.reset(); }

  void OnComplete(int error,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    DCHECK(!complete_);

    complete_ = true;
    top_level_result_error_ = error;
    result_error_ = resolve_error_info.error;
    result_addresses_ = addresses;
    endpoint_results_with_metadata_ = endpoint_results_with_metadata;
    if (run_loop_)
      run_loop_->Quit();
  }

  void OnTextResults(const std::vector<std::string>& text_results) override {
    DCHECK(!complete_);
    result_text_ = text_results;
  }

  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override {
    DCHECK(!complete_);
    result_hosts_ = hosts;
  }

  bool complete() const { return complete_; }

  int top_level_result_error() const {
    DCHECK(complete_);
    return top_level_result_error_;
  }

  int result_error() const {
    DCHECK(complete_);
    return result_error_;
  }

  const std::optional<net::AddressList>& result_addresses() const {
    DCHECK(complete_);
    return result_addresses_;
  }

  const std::optional<std::vector<std::string>>& result_text() const {
    DCHECK(complete_);
    return result_text_;
  }

  const std::optional<std::vector<net::HostPortPair>>& result_hosts() const {
    DCHECK(complete_);
    return result_hosts_;
  }

  const std::optional<net::HostResolverEndpointResults>&
  endpoint_results_with_metadata() const {
    DCHECK(complete_);
    return endpoint_results_with_metadata_;
  }

 private:
  mojo::Receiver<mojom::ResolveHostClient> receiver_{this};

  bool complete_;
  int top_level_result_error_;
  int result_error_;
  std::optional<net::AddressList> result_addresses_;
  std::optional<std::vector<std::string>> result_text_;
  std::optional<std::vector<net::HostPortPair>> result_hosts_;
  std::optional<net::HostResolverEndpointResults>
      endpoint_results_with_metadata_;
  const raw_ptr<base::RunLoop> run_loop_;
};

class TestMdnsListenClient : public mojom::MdnsListenClient {
 public:
  using UpdateType = net::MdnsListenerUpdateType;
  using UpdateKey = std::pair<UpdateType, net::DnsQueryType>;

  explicit TestMdnsListenClient(
      mojo::PendingRemote<mojom::MdnsListenClient>* remote)
      : receiver_(this, remote->InitWithNewPipeAndPassReceiver()) {}

  void OnAddressResult(UpdateType update_type,
                       net::DnsQueryType result_type,
                       const net::IPEndPoint& address) override {
    address_results_.insert({{update_type, result_type}, address});
  }

  void OnTextResult(UpdateType update_type,
                    net::DnsQueryType result_type,
                    const std::vector<std::string>& text_records) override {
    for (auto& text_record : text_records) {
      text_results_.insert({{update_type, result_type}, text_record});
    }
  }

  void OnHostnameResult(UpdateType update_type,
                        net::DnsQueryType result_type,
                        const net::HostPortPair& host) override {
    hostname_results_.insert({{update_type, result_type}, host});
  }

  void OnUnhandledResult(UpdateType update_type,
                         net::DnsQueryType result_type) override {
    unhandled_results_.insert({update_type, result_type});
  }

  const std::multimap<UpdateKey, net::IPEndPoint>& address_results() {
    return address_results_;
  }

  const std::multimap<UpdateKey, std::string>& text_results() {
    return text_results_;
  }

  const std::multimap<UpdateKey, net::HostPortPair>& hostname_results() {
    return hostname_results_;
  }

  const std::multiset<UpdateKey>& unhandled_results() {
    return unhandled_results_;
  }

  template <typename T>
  static std::pair<UpdateKey, T> CreateExpectedResult(
      UpdateType update_type,
      net::DnsQueryType query_type,
      T result) {
    return std::make_pair(std::make_pair(update_type, query_type), result);
  }

 private:
  mojo::Receiver<mojom::MdnsListenClient> receiver_;

  std::multimap<UpdateKey, net::IPEndPoint> address_results_;
  std::multimap<UpdateKey, std::string> text_results_;
  std::multimap<UpdateKey, net::HostPortPair> hostname_results_;
  std::multiset<UpdateKey> unhandled_results_;
};

TEST_F(HostResolverTest, Sync) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->set_synchronous_mode(true);
  inner_resolver->rules()->AddRule("example.test", "1.2.3.4");

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.test", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.top_level_result_error());
  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("1.2.3.4", 160)));
  EXPECT_FALSE(response_client.result_text());
  EXPECT_FALSE(response_client.result_hosts());
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_EQ(net::DEFAULT_PRIORITY, inner_resolver->last_request_priority());
}

TEST_F(HostResolverTest, Async) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->set_synchronous_mode(false);
  inner_resolver->rules()->AddRule("example.test", "1.2.3.4");

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.test", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("1.2.3.4", 160)));
  EXPECT_FALSE(response_client.result_text());
  EXPECT_FALSE(response_client.result_hosts());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_EQ(net::DEFAULT_PRIORITY, inner_resolver->last_request_priority());
}

TEST_F(HostResolverTest, DnsQueryType) {
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::DnsQueryType::AAAA;

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("::1", 160)));
}

TEST_F(HostResolverTest, InitialPriority) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddRule("priority.test", "1.2.3.4");

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->initial_priority = net::HIGHEST;

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("priority.test", 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("1.2.3.4", 80)));
  EXPECT_EQ(net::HIGHEST, inner_resolver->last_request_priority());
}

// Make requests specifying a source for host resolution and ensure the correct
// source is requested from the inner resolver.
TEST_F(HostResolverTest, Source) {
  constexpr char kDomain[] = "example.com";
  constexpr char kAnyResult[] = "1.2.3.4";
  constexpr char kSystemResult[] = "127.0.0.1";
  constexpr char kDnsResult[] = "168.100.12.23";
  constexpr char kMdnsResult[] = "200.1.2.3";
  auto inner_resolver = std::make_unique<net::MockHostResolver>();

  net::MockHostResolverBase::RuleResolver::RuleKey any_key;
  any_key.hostname_pattern = kDomain;
  any_key.query_source = net::HostResolverSource::ANY;
  inner_resolver->rules()->AddRule(std::move(any_key), kAnyResult);

  net::MockHostResolverBase::RuleResolver::RuleKey system_key;
  system_key.hostname_pattern = kDomain;
  system_key.query_source = net::HostResolverSource::SYSTEM;
  inner_resolver->rules()->AddRule(std::move(system_key), kSystemResult);

  net::MockHostResolverBase::RuleResolver::RuleKey dns_key;
  dns_key.hostname_pattern = kDomain;
  dns_key.query_source = net::HostResolverSource::DNS;
  inner_resolver->rules()->AddRule(std::move(dns_key), kDnsResult);

  net::MockHostResolverBase::RuleResolver::RuleKey mdns_key;
  mdns_key.hostname_pattern = kDomain;
  mdns_key.query_source = net::HostResolverSource::MULTICAST_DNS;
  inner_resolver->rules()->AddRule(std::move(mdns_key), kMdnsResult);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop any_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_any_client;
  TestResolveHostClient any_client(&pending_any_client, &any_run_loop);
  mojom::ResolveHostParametersPtr any_parameters =
      mojom::ResolveHostParameters::New();
  any_parameters->source = net::HostResolverSource::ANY;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(any_parameters),
                       std::move(pending_any_client));

  base::RunLoop system_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_system_client;
  TestResolveHostClient system_client(&pending_system_client, &system_run_loop);
  mojom::ResolveHostParametersPtr system_parameters =
      mojom::ResolveHostParameters::New();
  system_parameters->source = net::HostResolverSource::SYSTEM;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(system_parameters),
                       std::move(pending_system_client));

  base::RunLoop dns_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_dns_client;
  TestResolveHostClient dns_client(&pending_dns_client, &dns_run_loop);
  mojom::ResolveHostParametersPtr dns_parameters =
      mojom::ResolveHostParameters::New();
  dns_parameters->source = net::HostResolverSource::DNS;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(dns_parameters),
                       std::move(pending_dns_client));

  any_run_loop.Run();
  system_run_loop.Run();
  dns_run_loop.Run();

  EXPECT_EQ(net::OK, any_client.result_error());
  EXPECT_THAT(any_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kAnyResult, 80)));
  EXPECT_EQ(net::OK, system_client.result_error());
  EXPECT_THAT(system_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kSystemResult, 80)));
  EXPECT_EQ(net::OK, dns_client.result_error());
  EXPECT_THAT(dns_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kDnsResult, 80)));

#if BUILDFLAG(ENABLE_MDNS)
  base::RunLoop mdns_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_mdns_client;
  TestResolveHostClient mdns_client(&pending_mdns_client, &mdns_run_loop);
  mojom::ResolveHostParametersPtr mdns_parameters =
      mojom::ResolveHostParameters::New();
  mdns_parameters->source = net::HostResolverSource::MULTICAST_DNS;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(mdns_parameters),
                       std::move(pending_mdns_client));

  mdns_run_loop.Run();

  EXPECT_EQ(net::OK, mdns_client.result_error());
  EXPECT_THAT(mdns_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kMdnsResult, 80)));
#endif  // BUILDFLAG(ENABLE_MDNS)
}

// Make host resolve requests specifying https scheme and
// test that a resolver successfully gets https record information.
TEST_F(HostResolverTest, GetEndpointResultsWithMetadata) {
  using RuleResolver = net::MockHostResolverBase::RuleResolver;

  constexpr char kWithoutHttpsDomain[] = "without_https_record.test";
  constexpr char kWithHttpsDomain[] = "with_https_record.test";
  constexpr char kWithoutHttpsResult[] = "192.168.0.2";
  auto inner_resolver = std::make_unique<net::MockHostResolver>();

  RuleResolver::RuleKey without_https_key;
  without_https_key.hostname_pattern = kWithoutHttpsDomain;
  without_https_key.query_source = net::HostResolverSource::ANY;
  inner_resolver->rules()->AddRule(std::move(without_https_key),
                                   kWithoutHttpsResult);

  RuleResolver::RuleKey with_https_key;
  with_https_key.hostname_pattern = kWithHttpsDomain;
  with_https_key.query_source = net::HostResolverSource::ANY;

  net::HostResolverEndpointResults endpoint_results(2);
  const net::IPEndPoint with_https_ip_endpoint =
      net::IPEndPoint(net::IPAddress(192, 168, 0, 3), 443);
  net::ConnectionEndpointMetadata with_https_endpoint_metadata;
  with_https_endpoint_metadata.supported_protocol_alpns = {"http/1.1", "h2",
                                                           "h3"};
  endpoint_results[0].ip_endpoints = {with_https_ip_endpoint};
  endpoint_results[0].metadata = with_https_endpoint_metadata;
  // The last element of endpoint_results is non-protocol addresses.
  endpoint_results[1].ip_endpoints = {with_https_ip_endpoint};

  inner_resolver->rules()->AddRule(
      std::move(with_https_key),
      RuleResolver::RuleResult(std::move(endpoint_results)));

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop without_https_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_without_https_client;
  TestResolveHostClient without_https_client(&pending_without_https_client,
                                             &without_https_run_loop);
  auto without_https_scheme_host_port =
      url::SchemeHostPort("https", kWithoutHttpsDomain, 443);
  resolver.ResolveHost(network::mojom::HostResolverHost::NewSchemeHostPort(
                           without_https_scheme_host_port),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_without_https_client));

  base::RunLoop with_https_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_with_https_client;
  TestResolveHostClient with_https_client(&pending_with_https_client,
                                          &with_https_run_loop);
  auto with_https_scheme_host_port =
      url::SchemeHostPort("https", kWithHttpsDomain, 443);
  resolver.ResolveHost(network::mojom::HostResolverHost::NewSchemeHostPort(
                           with_https_scheme_host_port),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_with_https_client));

  without_https_run_loop.Run();
  with_https_run_loop.Run();

  net::HostResolverEndpointResults expected_endpoint_results(1);
  const net::IPEndPoint expected_with_https_ip_endpoint =
      net::IPEndPoint(net::IPAddress(192, 168, 0, 3), 443);
  net::ConnectionEndpointMetadata expected_with_https_endpoint_metadata;
  expected_with_https_endpoint_metadata.supported_protocol_alpns = {"http/1.1",
                                                                    "h2", "h3"};
  expected_endpoint_results[0].ip_endpoints = {expected_with_https_ip_endpoint};
  expected_endpoint_results[0].metadata = expected_with_https_endpoint_metadata;

  EXPECT_EQ(net::OK, without_https_client.result_error());
  EXPECT_THAT(
      without_https_client.endpoint_results_with_metadata(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));

  EXPECT_EQ(net::OK, with_https_client.result_error());
  EXPECT_THAT(with_https_client.endpoint_results_with_metadata(),
              expected_endpoint_results);
}

// Test that cached results are properly keyed by requested source.
TEST_F(HostResolverTest, SeparateCacheBySource) {
  constexpr char kDomain[] = "example.com";
  constexpr char kAnyResultOriginal[] = "1.2.3.4";
  constexpr char kSystemResultOriginal[] = "127.0.0.1";
  auto inner_resolver = std::make_unique<net::MockCachingHostResolver>();
  net::MockHostResolverBase::RuleResolver::RuleKey any_key;
  any_key.hostname_pattern = kDomain;
  any_key.query_source = net::HostResolverSource::ANY;
  inner_resolver->rules()->AddRule(any_key, kAnyResultOriginal);
  net::MockHostResolverBase::RuleResolver::RuleKey system_key;
  system_key.hostname_pattern = kDomain;
  system_key.query_source = net::HostResolverSource::SYSTEM;
  inner_resolver->rules()->AddRule(system_key, kSystemResultOriginal);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  // Load SYSTEM result into cache.
  base::RunLoop system_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_system_client_ptr;
  TestResolveHostClient system_client(&pending_system_client_ptr,
                                      &system_run_loop);
  mojom::ResolveHostParametersPtr system_parameters =
      mojom::ResolveHostParameters::New();
  system_parameters->source = net::HostResolverSource::SYSTEM;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(system_parameters),
                       std::move(pending_system_client_ptr));
  system_run_loop.Run();
  ASSERT_EQ(net::OK, system_client.result_error());
  EXPECT_THAT(
      system_client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kSystemResultOriginal, 80)));

  // Change |inner_resolver| rules to ensure results are coming from cache or
  // not based on whether they resolve to the old or new value.
  constexpr char kAnyResultFresh[] = "111.222.1.1";
  constexpr char kSystemResultFresh[] = "111.222.1.2";
  inner_resolver->rules()->ClearRules();
  inner_resolver->rules()->AddRule(any_key, kAnyResultFresh);
  inner_resolver->rules()->AddRule(system_key, kSystemResultFresh);

  base::RunLoop cached_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_cached_client;
  TestResolveHostClient cached_client(&pending_cached_client, &cached_run_loop);
  mojom::ResolveHostParametersPtr cached_parameters =
      mojom::ResolveHostParameters::New();
  cached_parameters->source = net::HostResolverSource::SYSTEM;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(cached_parameters),
                       std::move(pending_cached_client));

  base::RunLoop uncached_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_uncached_client;
  TestResolveHostClient uncached_client(&pending_uncached_client,
                                        &uncached_run_loop);
  mojom::ResolveHostParametersPtr uncached_parameters =
      mojom::ResolveHostParameters::New();
  uncached_parameters->source = net::HostResolverSource::ANY;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(uncached_parameters),
                       std::move(pending_uncached_client));

  cached_run_loop.Run();
  uncached_run_loop.Run();

  EXPECT_EQ(net::OK, cached_client.result_error());
  EXPECT_THAT(
      cached_client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kSystemResultOriginal, 80)));
  EXPECT_EQ(net::OK, uncached_client.result_error());
  EXPECT_THAT(
      uncached_client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kAnyResultFresh, 80)));
}

TEST_F(HostResolverTest, CacheDisabled) {
  constexpr char kDomain[] = "example.com";
  constexpr char kResultOriginal[] = "1.2.3.4";
  auto inner_resolver = std::make_unique<net::MockCachingHostResolver>();
  inner_resolver->rules()->AddRule(kDomain, kResultOriginal);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  // Load result into cache.
  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_client;
  TestResolveHostClient client(&pending_client, &run_loop);
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_client));
  run_loop.Run();
  ASSERT_EQ(net::OK, client.result_error());
  EXPECT_THAT(
      client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kResultOriginal, 80)));

  // Change |inner_resolver| rules to ensure results are coming from cache or
  // not based on whether they resolve to the old or new value.
  constexpr char kResultFresh[] = "111.222.1.1";
  inner_resolver->rules()->ClearRules();
  inner_resolver->rules()->AddRule(kDomain, kResultFresh);

  base::RunLoop cached_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_cached_client;
  TestResolveHostClient cached_client(&pending_cached_client, &cached_run_loop);
  mojom::ResolveHostParametersPtr cached_parameters =
      mojom::ResolveHostParameters::New();
  cached_parameters->cache_usage =
      mojom::ResolveHostParameters::CacheUsage::ALLOWED;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(cached_parameters),
                       std::move(pending_cached_client));
  cached_run_loop.Run();

  EXPECT_EQ(net::OK, cached_client.result_error());
  EXPECT_THAT(
      cached_client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kResultOriginal, 80)));

  base::RunLoop uncached_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_uncached_client;
  TestResolveHostClient uncached_client(&pending_uncached_client,
                                        &uncached_run_loop);
  mojom::ResolveHostParametersPtr uncached_parameters =
      mojom::ResolveHostParameters::New();
  uncached_parameters->cache_usage =
      mojom::ResolveHostParameters::CacheUsage::DISALLOWED;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(uncached_parameters),
                       std::move(pending_uncached_client));
  uncached_run_loop.Run();

  EXPECT_EQ(net::OK, uncached_client.result_error());
  EXPECT_THAT(uncached_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kResultFresh, 80)));
}

TEST_F(HostResolverTest, CacheStaleAllowed) {
  constexpr char kDomain[] = "example.com";
  constexpr char kResultOriginal[] = "1.2.3.4";
  auto inner_resolver = std::make_unique<net::MockCachingHostResolver>();
  inner_resolver->rules()->AddRule(kDomain, kResultOriginal);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  // Load result into cache.
  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_client;
  TestResolveHostClient client(&pending_client, &run_loop);
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_client));
  run_loop.Run();
  ASSERT_EQ(net::OK, client.result_error());
  EXPECT_THAT(
      client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kResultOriginal, 80)));

  // Change |inner_resolver| rules to ensure results are coming from cache or
  // not based on whether they resolve to the old or new value.
  constexpr char kResultFresh[] = "111.222.1.1";
  inner_resolver->rules()->ClearRules();
  inner_resolver->rules()->AddRule(kDomain, kResultFresh);

  // MockHostResolver gives cache entries a 1 min TTL, so simulate a day
  // passing, which is more than long enough for the cached results to become
  // stale.
  task_environment_.FastForwardBy(base::Days(1));

  // Fetching stale results returns the original cached value.
  base::RunLoop cached_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_cached_client;
  TestResolveHostClient cached_client(&pending_cached_client, &cached_run_loop);
  mojom::ResolveHostParametersPtr cached_parameters =
      mojom::ResolveHostParameters::New();
  cached_parameters->cache_usage =
      mojom::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(cached_parameters),
                       std::move(pending_cached_client));
  cached_run_loop.Run();

  EXPECT_EQ(net::OK, cached_client.result_error());
  EXPECT_THAT(
      cached_client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kResultOriginal, 80)));

  // Resolution where only non-stale cache usage is allowed returns the new
  // value.
  base::RunLoop uncached_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_uncached_client;
  TestResolveHostClient uncached_client(&pending_uncached_client,
                                        &uncached_run_loop);
  mojom::ResolveHostParametersPtr uncached_parameters =
      mojom::ResolveHostParameters::New();
  uncached_parameters->cache_usage =
      mojom::ResolveHostParameters::CacheUsage::ALLOWED;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(uncached_parameters),
                       std::move(pending_uncached_client));
  uncached_run_loop.Run();

  EXPECT_EQ(net::OK, uncached_client.result_error());
  EXPECT_THAT(uncached_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kResultFresh, 80)));
}

// Test for a resolve with a result only in the cache and error if the cache is
// disabled.
TEST_F(HostResolverTest, CacheDisabled_ErrorResults) {
  constexpr char kDomain[] = "example.com";
  constexpr char kResult[] = "1.2.3.4";
  auto inner_resolver = std::make_unique<net::MockCachingHostResolver>();
  inner_resolver->rules()->AddRule(kDomain, kResult);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  // Load initial result into cache.
  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_client;
  TestResolveHostClient client(&pending_client, &run_loop);
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_client));
  run_loop.Run();
  ASSERT_EQ(net::OK, client.result_error());

  // Change |inner_resolver| rules to an error.
  inner_resolver->rules()->ClearRules();
  inner_resolver->rules()->AddSimulatedFailure(kDomain);

  // Resolves for |kFreshErrorDomain| should result in error only when cache is
  // disabled because success was cached.
  base::RunLoop cached_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_cached_client;
  TestResolveHostClient cached_client(&pending_cached_client, &cached_run_loop);
  mojom::ResolveHostParametersPtr cached_parameters =
      mojom::ResolveHostParameters::New();
  cached_parameters->cache_usage =
      mojom::ResolveHostParameters::CacheUsage::ALLOWED;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(cached_parameters),
                       std::move(pending_cached_client));
  cached_run_loop.Run();
  EXPECT_EQ(net::OK, cached_client.result_error());

  base::RunLoop uncached_run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_uncached_client;
  TestResolveHostClient uncached_client(&pending_uncached_client,
                                        &uncached_run_loop);
  mojom::ResolveHostParametersPtr uncached_parameters =
      mojom::ResolveHostParameters::New();
  uncached_parameters->cache_usage =
      mojom::ResolveHostParameters::CacheUsage::DISALLOWED;
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair(kDomain, 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(uncached_parameters),
                       std::move(pending_uncached_client));
  uncached_run_loop.Run();
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, uncached_client.result_error());
}

TEST_F(HostResolverTest, IncludeCanonicalName) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddRuleWithFlags("example.com", "123.0.12.24",
                                            net::HOST_RESOLVER_CANONNAME,
                                            {"canonicalexample.com"});

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->include_canonical_name = true;

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.com", 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("123.0.12.24", 80)));
  EXPECT_THAT(response_client.result_addresses().value().dns_aliases(),
              testing::ElementsAre("canonicalexample.com"));
}

TEST_F(HostResolverTest, LoopbackOnly) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddRuleWithFlags("example.com", "127.0.12.24",
                                            net::HOST_RESOLVER_LOOPBACK_ONLY);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->loopback_only = true;

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.com", 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("127.0.12.24", 80)));
}

TEST_F(HostResolverTest, HandlesSecureDnsPolicyParameter) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddRule("secure.test", "1.2.3.4");

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->secure_dns_policy =
      network::mojom::SecureDnsPolicy::DISABLE;

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("secure.test", 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("1.2.3.4", 80)));
  EXPECT_EQ(net::SecureDnsPolicy::kDisable,
            inner_resolver->last_secure_dns_policy());
}

TEST_F(HostResolverTest, Failure_Sync) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddSimulatedFailure("example.com");
  inner_resolver->set_synchronous_mode(true);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.com", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED,
            response_client.top_level_result_error());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Failure_Async) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddSimulatedFailure("example.com");
  inner_resolver->set_synchronous_mode(false);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.com", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, NetworkAnonymizationKey) {
  const net::SchemefulSite kSite =
      net::SchemefulSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateSameSite(kSite);

  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddRule("nik.test", "1.2.3.4");

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("nik.test", 160)),
                       kNetworkAnonymizationKey, std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("1.2.3.4", 160)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_EQ(kNetworkAnonymizationKey,
            inner_resolver->last_request_network_anonymization_key());
}

TEST_F(HostResolverTest, NoOptionalParameters) {
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, NoControlHandle) {
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, CloseControlHandle) {
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  control_handle.reset();
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160),
                                    CreateExpectedEndPoint("::1", 160)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Cancellation) {
  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  control_handle->Cancel(net::ERR_ABORTED);
  run_loop.Run();

  // On cancellation, should receive an ERR_FAILED result, and the internal
  // resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_ABORTED, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Cancellation_SubsequentRequest) {
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, nullptr);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));

  control_handle->Cancel(net::ERR_ABORTED);
  run_loop.RunUntilIdle();

  // Not using a hanging resolver, so could be ERR_ABORTED or OK depending on
  // timing of the cancellation.
  EXPECT_TRUE(response_client.result_error() == net::ERR_ABORTED ||
              response_client.result_error() == net::OK);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Subsequent requests should be unaffected by the cancellation.
  base::RunLoop run_loop2;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client2;
  TestResolveHostClient response_client2(&pending_response_client2, &run_loop2);
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_response_client2));
  run_loop2.Run();

  EXPECT_EQ(net::OK, response_client2.result_error());
  EXPECT_THAT(
      response_client2.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, DestroyResolver) {
  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();

  auto resolver =
      std::make_unique<HostResolver>(inner_resolver.get(), net::NetLog::Get());

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                            net::HostPortPair("localhost", 80)),
                        net::NetworkAnonymizationKey(),
                        std::move(optional_parameters),
                        std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  resolver = nullptr;
  run_loop.Run();

  // On context destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
}

TEST_F(HostResolverTest, CloseClient) {
  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  response_client.CloseReceiver();
  run_loop.RunUntilIdle();

  // Response pipe is closed, so no results to check. Internal request should be
  // cancelled.
  EXPECT_FALSE(response_client.complete());
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, CloseClient_SubsequentRequest) {
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, nullptr);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_response_client));

  response_client.CloseReceiver();
  run_loop.RunUntilIdle();

  // Not using a hanging resolver, so could be incomplete or OK depending on
  // timing of the cancellation.
  EXPECT_TRUE(!response_client.complete() ||
              response_client.result_error() == net::OK);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Subsequent requests should be unaffected by the cancellation.
  base::RunLoop run_loop2;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client2;
  TestResolveHostClient response_client2(&pending_response_client2, &run_loop2);
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_response_client2));
  run_loop2.Run();

  EXPECT_EQ(net::OK, response_client2.result_error());
  EXPECT_THAT(
      response_client2.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Binding) {
  mojo::Remote<mojom::HostResolver> resolver_remote;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(resolver_remote.BindNewPipeAndPassReceiver(),
                        std::move(shutdown_callback), inner_resolver.get(),
                        /*owned_internal_resolver=*/nullptr,
                        net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver_remote->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 160)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 160),
                                    CreateExpectedEndPoint("::1", 160)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_FALSE(shutdown_resolver);
}

TEST_F(HostResolverTest, CloseBinding) {
  mojo::Remote<mojom::HostResolver> resolver_remote;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();

  HostResolver resolver(resolver_remote.BindNewPipeAndPassReceiver(),
                        std::move(shutdown_callback), inner_resolver.get(),
                        /*owned_internal_resolver=*/nullptr,
                        net::NetLog::Get());

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);
  resolver_remote->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 160)),
      net::NetworkAnonymizationKey(), std::move(optional_parameters),
      std::move(pending_response_client));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_disconnect_handler(connection_error_callback);

  resolver_remote.reset();
  run_loop.Run();

  // Request should be cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Callback should have been called.
  EXPECT_EQ(&resolver, shutdown_resolver);
}

TEST_F(HostResolverTest, CloseBinding_SubsequentRequest) {
  mojo::Remote<mojom::HostResolver> resolver_remote;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(resolver_remote.BindNewPipeAndPassReceiver(),
                        std::move(shutdown_callback), inner_resolver.get(),
                        /*owned_internal_resolver=*/nullptr,
                        net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, nullptr);
  resolver_remote->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair("localhost", 160)),
      net::NetworkAnonymizationKey(), nullptr,
      std::move(pending_response_client));

  resolver_remote.reset();
  run_loop.RunUntilIdle();

  // Not using a hanging resolver, so could be ERR_FAILED or OK depending on
  // timing of the cancellation.
  EXPECT_TRUE(response_client.result_error() == net::ERR_FAILED ||
              response_client.result_error() == net::OK);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Callback should have been called.
  EXPECT_EQ(&resolver, shutdown_resolver);

  // Subsequent requests should be unaffected by the cancellation.
  base::RunLoop run_loop2;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client2;
  TestResolveHostClient response_client2(&pending_response_client2, &run_loop2);
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(), nullptr,
                       std::move(pending_response_client2));
  run_loop2.Run();

  EXPECT_EQ(net::OK, response_client2.result_error());
  EXPECT_THAT(
      response_client2.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, IsSpeculative) {
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneResolver(net::NetLog::Get());

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);
  mojom::ResolveHostParametersPtr parameters =
      mojom::ResolveHostParameters::New();
  parameters->is_speculative = true;

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("localhost", 80)),
                       net::NetworkAnonymizationKey(), std::move(parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

net::DnsConfig CreateValidDnsConfig() {
  net::IPAddress dns_ip(192, 168, 1, 0);
  net::DnsConfig config;
  config.nameservers.emplace_back(dns_ip, net::dns_protocol::kDefaultPort);
  EXPECT_TRUE(config.IsValid());
  return config;
}

TEST_F(HostResolverTest, TextResults) {
  static const char* kTextRecords[] = {"foo", "bar", "more text"};
  net::MockDnsClientRuleList rules;
  rules.emplace_back(
      "example.com", net::dns_protocol::kTypeTXT, false /* secure */,
      net::MockDnsClientRule::Result(net::BuildTestDnsTextResponse(
          "example.com", {std::vector<std::string>(std::begin(kTextRecords),
                                                   std::end(kTextRecords))})),
      false /* delay */);
  auto dns_client = std::make_unique<net::MockDnsClient>(CreateValidDnsConfig(),
                                                         std::move(rules));
  dns_client->set_ignore_system_config_changes(true);

  std::unique_ptr<net::ContextHostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneContextResolver(net::NetLog::Get());
  inner_resolver->GetManagerForTesting()->SetDnsClientForTesting(
      std::move(dns_client));
  inner_resolver->GetManagerForTesting()->SetInsecureDnsClientEnabled(
      /*enabled=*/true,
      /*additional_dns_types_enabled=*/true);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::DnsQueryType::TXT;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.com", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));
  EXPECT_THAT(response_client.result_text(),
              testing::Optional(testing::ElementsAreArray(kTextRecords)));
  EXPECT_FALSE(response_client.result_hosts());
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, HostResults) {
  net::MockDnsClientRuleList rules;
  rules.emplace_back(
      "example.com", net::dns_protocol::kTypePTR, false /*secure */,
      net::MockDnsClientRule::Result(net::BuildTestDnsPointerResponse(
          "example.com", {"google.com", "chromium.org"})),
      false /* delay */);
  auto dns_client = std::make_unique<net::MockDnsClient>(CreateValidDnsConfig(),
                                                         std::move(rules));
  dns_client->set_ignore_system_config_changes(true);

  std::unique_ptr<net::ContextHostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneContextResolver(net::NetLog::Get());
  inner_resolver->GetManagerForTesting()->SetDnsClientForTesting(
      std::move(dns_client));
  inner_resolver->GetManagerForTesting()->SetInsecureDnsClientEnabled(
      /*enabled=*/true,
      /*additional_dns_types_enabled=*/true);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::DnsQueryType::PTR;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.com", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses(),
      testing::AnyOf(std::nullopt, testing::Optional(testing::IsEmpty())));
  EXPECT_FALSE(response_client.result_text());
  EXPECT_THAT(response_client.result_hosts(),
              testing::Optional(testing::UnorderedElementsAre(
                  net::HostPortPair("google.com", 160),
                  net::HostPortPair("chromium.org", 160))));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, RespectsDisablingAdditionalQueryTypes) {
  net::MockDnsClientRuleList rules;
  auto dns_client = std::make_unique<net::MockDnsClient>(CreateValidDnsConfig(),
                                                         std::move(rules));
  dns_client->set_ignore_system_config_changes(true);

  std::unique_ptr<net::ContextHostResolver> inner_resolver =
      net::HostResolver::CreateStandaloneContextResolver(net::NetLog::Get());
  inner_resolver->GetManagerForTesting()->SetDnsClientForTesting(
      std::move(dns_client));
  inner_resolver->GetManagerForTesting()->SetInsecureDnsClientEnabled(
      /*enabled=*/true,
      /*additional_dns_types_enabled=*/false);

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::DnsQueryType::PTR;
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("example.com", 160)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  // No queries made, so result is `ERR_DNS_CACHE_MISS`.
  EXPECT_THAT(response_client.result_error(),
              net::test::IsError(net::ERR_DNS_CACHE_MISS));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, CanonicalizesInputHost) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->set_synchronous_mode(true);
  inner_resolver->rules()->AddRule("name.test", "1.2.3.4");

  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  base::RunLoop run_loop;
  mojo::Remote<mojom::ResolveHostHandle> control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle =
      control_handle.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client;
  TestResolveHostClient response_client(&pending_response_client, &run_loop);

  resolver.ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                           net::HostPortPair("NaMe.test", 165)),
                       net::NetworkAnonymizationKey(),
                       std::move(optional_parameters),
                       std::move(pending_response_client));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.top_level_result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("1.2.3.4", 165)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

#if BUILDFLAG(ENABLE_MDNS)
TEST_F(HostResolverTest, MdnsListener_AddressResult) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojo::PendingRemote<mojom::MdnsListenClient> pending_response_client;
  TestMdnsListenClient response_client(&pending_response_client);

  int error = net::ERR_FAILED;
  base::RunLoop run_loop;
  net::HostPortPair host("host.local", 41);
  resolver.MdnsListen(host, net::DnsQueryType::A,
                      std::move(pending_response_client),
                      base::BindLambdaForTesting([&](int error_val) {
                        error = error_val;
                        run_loop.Quit();
                      }));

  run_loop.Run();
  ASSERT_EQ(net::OK, error);

  net::IPAddress result_address(1, 2, 3, 4);
  net::IPEndPoint result(result_address, 41);
  inner_resolver->TriggerMdnsListeners(
      host, net::DnsQueryType::A, net::MdnsListenerUpdateType::kAdded, result);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      response_client.address_results(),
      testing::ElementsAre(TestMdnsListenClient::CreateExpectedResult(
          net::MdnsListenerUpdateType::kAdded, net::DnsQueryType::A, result)));

  EXPECT_THAT(response_client.text_results(), testing::IsEmpty());
  EXPECT_THAT(response_client.hostname_results(), testing::IsEmpty());
  EXPECT_THAT(response_client.unhandled_results(), testing::IsEmpty());
}

TEST_F(HostResolverTest, MdnsListener_TextResult) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojo::PendingRemote<mojom::MdnsListenClient> pending_response_client;
  TestMdnsListenClient response_client(&pending_response_client);

  int error = net::ERR_FAILED;
  base::RunLoop run_loop;
  net::HostPortPair host("host.local", 42);
  resolver.MdnsListen(host, net::DnsQueryType::TXT,
                      std::move(pending_response_client),
                      base::BindLambdaForTesting([&](int error_val) {
                        error = error_val;
                        run_loop.Quit();
                      }));

  run_loop.Run();
  ASSERT_EQ(net::OK, error);

  inner_resolver->TriggerMdnsListeners(host, net::DnsQueryType::TXT,
                                       net::MdnsListenerUpdateType::kChanged,
                                       {"foo", "bar"});
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      response_client.text_results(),
      testing::UnorderedElementsAre(TestMdnsListenClient::CreateExpectedResult(
                                        net::MdnsListenerUpdateType::kChanged,
                                        net::DnsQueryType::TXT, "foo"),
                                    TestMdnsListenClient::CreateExpectedResult(
                                        net::MdnsListenerUpdateType::kChanged,
                                        net::DnsQueryType::TXT, "bar")));

  EXPECT_THAT(response_client.address_results(), testing::IsEmpty());
  EXPECT_THAT(response_client.hostname_results(), testing::IsEmpty());
  EXPECT_THAT(response_client.unhandled_results(), testing::IsEmpty());
}

TEST_F(HostResolverTest, MdnsListener_HostnameResult) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojo::PendingRemote<mojom::MdnsListenClient> pending_response_client;
  TestMdnsListenClient response_client(&pending_response_client);

  int error = net::ERR_FAILED;
  base::RunLoop run_loop;
  net::HostPortPair host("host.local", 43);
  resolver.MdnsListen(host, net::DnsQueryType::PTR,
                      std::move(pending_response_client),
                      base::BindLambdaForTesting([&](int error_val) {
                        error = error_val;
                        run_loop.Quit();
                      }));

  run_loop.Run();
  ASSERT_EQ(net::OK, error);

  net::HostPortPair result("example.com", 43);
  inner_resolver->TriggerMdnsListeners(host, net::DnsQueryType::PTR,
                                       net::MdnsListenerUpdateType::kRemoved,
                                       result);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(response_client.hostname_results(),
              testing::ElementsAre(TestMdnsListenClient::CreateExpectedResult(
                  net::MdnsListenerUpdateType::kRemoved, net::DnsQueryType::PTR,
                  result)));

  EXPECT_THAT(response_client.address_results(), testing::IsEmpty());
  EXPECT_THAT(response_client.text_results(), testing::IsEmpty());
  EXPECT_THAT(response_client.unhandled_results(), testing::IsEmpty());
}

TEST_F(HostResolverTest, MdnsListener_UnhandledResult) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  HostResolver resolver(inner_resolver.get(), net::NetLog::Get());

  mojo::PendingRemote<mojom::MdnsListenClient> pending_response_client;
  TestMdnsListenClient response_client(&pending_response_client);

  int error = net::ERR_FAILED;
  base::RunLoop run_loop;
  net::HostPortPair host("host.local", 44);
  resolver.MdnsListen(host, net::DnsQueryType::PTR,
                      std::move(pending_response_client),
                      base::BindLambdaForTesting([&](int error_val) {
                        error = error_val;
                        run_loop.Quit();
                      }));

  run_loop.Run();
  ASSERT_EQ(net::OK, error);

  inner_resolver->TriggerMdnsListeners(host, net::DnsQueryType::PTR,
                                       net::MdnsListenerUpdateType::kAdded);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      response_client.unhandled_results(),
      testing::ElementsAre(std::make_pair(net::MdnsListenerUpdateType::kAdded,
                                          net::DnsQueryType::PTR)));

  EXPECT_THAT(response_client.address_results(), testing::IsEmpty());
  EXPECT_THAT(response_client.text_results(), testing::IsEmpty());
  EXPECT_THAT(response_client.hostname_results(), testing::IsEmpty());
}
#endif  // BUILDFLAG(ENABLE_MDNS)

}  // namespace
}  // namespace network
