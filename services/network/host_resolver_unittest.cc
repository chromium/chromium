// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log.h"
#include "services/network/host_resolver.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class HostResolverTest : public testing::Test {
 public:
  HostResolverTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
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
  TestResolveHostClient(mojom::ResolveHostClientPtr* interface_ptr,
                        base::RunLoop* run_loop)
      : binding_(this, mojo::MakeRequest(interface_ptr)),
        complete_(false),
        result_error_(net::ERR_UNEXPECTED),
        run_loop_(run_loop) {}

  void CloseBinding() { binding_.Close(); }

  void OnComplete(int error,
                  const base::Optional<net::AddressList>& addresses) override {
    DCHECK(!complete_);

    complete_ = true;
    result_error_ = error;
    result_addresses_ = addresses;
    if (run_loop_)
      run_loop_->Quit();
  }

  bool complete() const { return complete_; }

  int result_error() const {
    DCHECK(complete_);
    return result_error_;
  }

  const base::Optional<net::AddressList>& result_addresses() const {
    DCHECK(complete_);
    return result_addresses_;
  }

 private:
  mojo::Binding<mojom::ResolveHostClient> binding_;

  bool complete_;
  int result_error_;
  base::Optional<net::AddressList> result_addresses_;
  base::RunLoop* const run_loop_;
};

TEST_F(HostResolverTest, Sync) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->set_synchronous_mode(true);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("127.0.0.1", 160)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_EQ(net::DEFAULT_PRIORITY, inner_resolver->last_request_priority());
}

TEST_F(HostResolverTest, Async) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->set_synchronous_mode(false);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("127.0.0.1", 160)));
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
  EXPECT_EQ(net::DEFAULT_PRIORITY, inner_resolver->last_request_priority());
}

TEST_F(HostResolverTest, DnsQueryType) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->dns_query_type = net::HostResolver::DnsQueryType::AAAA;

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("::1", 160)));
}

TEST_F(HostResolverTest, InitialPriority) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->initial_priority = net::HIGHEST;

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("127.0.0.1", 80)));
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
  inner_resolver->rules_map()[net::HostResolverSource::ANY]->AddRule(
      kDomain, kAnyResult);
  inner_resolver->rules_map()[net::HostResolverSource::SYSTEM]->AddRule(
      kDomain, kSystemResult);
  inner_resolver->rules_map()[net::HostResolverSource::DNS]->AddRule(
      kDomain, kDnsResult);
  inner_resolver->rules_map()[net::HostResolverSource::MULTICAST_DNS]->AddRule(
      kDomain, kMdnsResult);

  net::NetLog net_log;
  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop any_run_loop;
  mojom::ResolveHostClientPtr any_client_ptr;
  TestResolveHostClient any_client(&any_client_ptr, &any_run_loop);
  mojom::ResolveHostParametersPtr any_parameters =
      mojom::ResolveHostParameters::New();
  any_parameters->source = net::HostResolverSource::ANY;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(any_parameters), std::move(any_client_ptr));

  base::RunLoop system_run_loop;
  mojom::ResolveHostClientPtr system_client_ptr;
  TestResolveHostClient system_client(&system_client_ptr, &system_run_loop);
  mojom::ResolveHostParametersPtr system_parameters =
      mojom::ResolveHostParameters::New();
  system_parameters->source = net::HostResolverSource::SYSTEM;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(system_parameters),
                       std::move(system_client_ptr));

  base::RunLoop dns_run_loop;
  mojom::ResolveHostClientPtr dns_client_ptr;
  TestResolveHostClient dns_client(&dns_client_ptr, &dns_run_loop);
  mojom::ResolveHostParametersPtr dns_parameters =
      mojom::ResolveHostParameters::New();
  dns_parameters->source = net::HostResolverSource::DNS;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(dns_parameters), std::move(dns_client_ptr));

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
  mojom::ResolveHostClientPtr mdns_client_ptr;
  TestResolveHostClient mdns_client(&mdns_client_ptr, &mdns_run_loop);
  mojom::ResolveHostParametersPtr mdns_parameters =
      mojom::ResolveHostParameters::New();
  mdns_parameters->source = net::HostResolverSource::MULTICAST_DNS;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(mdns_parameters), std::move(mdns_client_ptr));

  mdns_run_loop.Run();

  EXPECT_EQ(net::OK, mdns_client.result_error());
  EXPECT_THAT(mdns_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint(kMdnsResult, 80)));
#endif  // BUILDFLAG(ENABLE_MDNS)
}

// Test that cached results are properly keyed by requested source.
TEST_F(HostResolverTest, SeparateCacheBySource) {
  constexpr char kDomain[] = "example.com";
  constexpr char kAnyResultOriginal[] = "1.2.3.4";
  constexpr char kSystemResultOriginal[] = "127.0.0.1";
  auto inner_resolver = std::make_unique<net::MockCachingHostResolver>();
  inner_resolver->rules_map()[net::HostResolverSource::ANY]->AddRule(
      kDomain, kAnyResultOriginal);
  inner_resolver->rules_map()[net::HostResolverSource::SYSTEM]->AddRule(
      kDomain, kSystemResultOriginal);
  base::SimpleTestTickClock test_clock;
  inner_resolver->set_tick_clock(&test_clock);

  net::NetLog net_log;
  HostResolver resolver(inner_resolver.get(), &net_log);

  // Load SYSTEM result into cache.
  base::RunLoop system_run_loop;
  mojom::ResolveHostClientPtr system_client_ptr;
  TestResolveHostClient system_client(&system_client_ptr, &system_run_loop);
  mojom::ResolveHostParametersPtr system_parameters =
      mojom::ResolveHostParameters::New();
  system_parameters->source = net::HostResolverSource::SYSTEM;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(system_parameters),
                       std::move(system_client_ptr));
  system_run_loop.Run();
  ASSERT_EQ(net::OK, system_client.result_error());
  EXPECT_THAT(
      system_client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kSystemResultOriginal, 80)));

  // Change |inner_resolver| rules to ensure results are coming from cache or
  // not based on whether they resolve to the old or new value.
  constexpr char kAnyResultFresh[] = "111.222.1.1";
  constexpr char kSystemResultFresh[] = "111.222.1.2";
  inner_resolver->rules_map()[net::HostResolverSource::ANY]->ClearRules();
  inner_resolver->rules_map()[net::HostResolverSource::ANY]->AddRule(
      kDomain, kAnyResultFresh);
  inner_resolver->rules_map()[net::HostResolverSource::SYSTEM]->ClearRules();
  inner_resolver->rules_map()[net::HostResolverSource::SYSTEM]->AddRule(
      kDomain, kSystemResultFresh);

  base::RunLoop cached_run_loop;
  mojom::ResolveHostClientPtr cached_client_ptr;
  TestResolveHostClient cached_client(&cached_client_ptr, &cached_run_loop);
  mojom::ResolveHostParametersPtr cached_parameters =
      mojom::ResolveHostParameters::New();
  cached_parameters->source = net::HostResolverSource::SYSTEM;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(cached_parameters),
                       std::move(cached_client_ptr));

  base::RunLoop uncached_run_loop;
  mojom::ResolveHostClientPtr uncached_client_ptr;
  TestResolveHostClient uncached_client(&uncached_client_ptr,
                                        &uncached_run_loop);
  mojom::ResolveHostParametersPtr uncached_parameters =
      mojom::ResolveHostParameters::New();
  uncached_parameters->source = net::HostResolverSource::ANY;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(uncached_parameters),
                       std::move(uncached_client_ptr));

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
  base::SimpleTestTickClock test_clock;
  inner_resolver->set_tick_clock(&test_clock);

  net::NetLog net_log;
  HostResolver resolver(inner_resolver.get(), &net_log);

  // Load result into cache.
  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr client_ptr;
  TestResolveHostClient client(&client_ptr, &run_loop);
  resolver.ResolveHost(net::HostPortPair(kDomain, 80), nullptr,
                       std::move(client_ptr));
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
  mojom::ResolveHostClientPtr cached_client_ptr;
  TestResolveHostClient cached_client(&cached_client_ptr, &cached_run_loop);
  mojom::ResolveHostParametersPtr cached_parameters =
      mojom::ResolveHostParameters::New();
  cached_parameters->allow_cached_response = true;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(cached_parameters),
                       std::move(cached_client_ptr));
  cached_run_loop.Run();

  EXPECT_EQ(net::OK, cached_client.result_error());
  EXPECT_THAT(
      cached_client.result_addresses().value().endpoints(),
      testing::ElementsAre(CreateExpectedEndPoint(kResultOriginal, 80)));

  base::RunLoop uncached_run_loop;
  mojom::ResolveHostClientPtr uncached_client_ptr;
  TestResolveHostClient uncached_client(&uncached_client_ptr,
                                        &uncached_run_loop);
  mojom::ResolveHostParametersPtr uncached_parameters =
      mojom::ResolveHostParameters::New();
  uncached_parameters->allow_cached_response = false;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(uncached_parameters),
                       std::move(uncached_client_ptr));
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
  base::SimpleTestTickClock test_clock;
  inner_resolver->set_tick_clock(&test_clock);

  net::NetLog net_log;
  HostResolver resolver(inner_resolver.get(), &net_log);

  // Load initial result into cache.
  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr client_ptr;
  TestResolveHostClient client(&client_ptr, &run_loop);
  resolver.ResolveHost(net::HostPortPair(kDomain, 80), nullptr,
                       std::move(client_ptr));
  run_loop.Run();
  ASSERT_EQ(net::OK, client.result_error());

  // Change |inner_resolver| rules to an error.
  inner_resolver->rules()->ClearRules();
  inner_resolver->rules()->AddSimulatedFailure(kDomain);

  // Resolves for |kFreshErrorDomain| should result in error only when cache is
  // disabled because success was cached.
  base::RunLoop cached_run_loop;
  mojom::ResolveHostClientPtr cached_client_ptr;
  TestResolveHostClient cached_client(&cached_client_ptr, &cached_run_loop);
  mojom::ResolveHostParametersPtr cached_parameters =
      mojom::ResolveHostParameters::New();
  cached_parameters->allow_cached_response = true;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(cached_parameters),
                       std::move(cached_client_ptr));
  cached_run_loop.Run();
  EXPECT_EQ(net::OK, cached_client.result_error());

  base::RunLoop uncached_run_loop;
  mojom::ResolveHostClientPtr uncached_client_ptr;
  TestResolveHostClient uncached_client(&uncached_client_ptr,
                                        &uncached_run_loop);
  mojom::ResolveHostParametersPtr uncached_parameters =
      mojom::ResolveHostParameters::New();
  uncached_parameters->allow_cached_response = false;
  resolver.ResolveHost(net::HostPortPair(kDomain, 80),
                       std::move(uncached_parameters),
                       std::move(uncached_client_ptr));
  uncached_run_loop.Run();
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, uncached_client.result_error());
}

TEST_F(HostResolverTest, IncludeCanonicalName) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddRuleWithFlags("example.com", "123.0.12.24",
                                            net::HOST_RESOLVER_CANONNAME,
                                            "canonicalexample.com");
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->include_canonical_name = true;

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("example.com", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("123.0.12.24", 80)));
  EXPECT_EQ("canonicalexample.com",
            response_client.result_addresses().value().canonical_name());
}

TEST_F(HostResolverTest, LoopbackOnly) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddRuleWithFlags("example.com", "127.0.12.24",
                                            net::HOST_RESOLVER_LOOPBACK_ONLY);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->loopback_only = true;

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("example.com", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(response_client.result_addresses().value().endpoints(),
              testing::ElementsAre(CreateExpectedEndPoint("127.0.12.24", 80)));
}

TEST_F(HostResolverTest, Failure_Sync) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddSimulatedFailure("example.com");
  inner_resolver->set_synchronous_mode(true);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("example.com", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Failure_Async) {
  auto inner_resolver = std::make_unique<net::MockHostResolver>();
  inner_resolver->rules()->AddSimulatedFailure("example.com");
  inner_resolver->set_synchronous_mode(false);
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("example.com", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));

  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);
  run_loop.Run();

  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, NoOptionalParameters) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, NoControlHandle) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_THAT(
      response_client.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, CloseControlHandle) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(net::HostPortPair("localhost", 160),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  control_handle = nullptr;
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
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  control_handle->Cancel(net::ERR_ABORTED);
  run_loop.Run();

  // On cancellation, should receive an ERR_FAILED result, and the internal
  // resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_ABORTED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Cancellation_SubsequentRequest) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, nullptr);

  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));

  control_handle->Cancel(net::ERR_ABORTED);
  run_loop.RunUntilIdle();

  // Not using a hanging resolver, so could be ERR_ABORTED or OK depending on
  // timing of the cancellation.
  EXPECT_TRUE(response_client.result_error() == net::ERR_ABORTED ||
              response_client.result_error() == net::OK);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Subsequent requests should be unaffected by the cancellation.
  base::RunLoop run_loop2;
  mojom::ResolveHostClientPtr response_client_ptr2;
  TestResolveHostClient response_client2(&response_client_ptr2, &run_loop2);
  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr2));
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
  net::NetLog net_log;

  auto resolver =
      std::make_unique<HostResolver>(inner_resolver.get(), &net_log);

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver->ResolveHost(net::HostPortPair("localhost", 80),
                        std::move(optional_parameters),
                        std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  resolver = nullptr;
  run_loop.Run();

  // On context destruction, should receive an ERR_FAILED result, and the
  // internal resolver request should have been cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
}

TEST_F(HostResolverTest, CloseClient) {
  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();
  net::NetLog net_log;

  HostResolver resolver(inner_resolver.get(), &net_log);

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(optional_parameters),
                       std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  response_client.CloseBinding();
  run_loop.RunUntilIdle();

  // Response pipe is closed, so no results to check. Internal request should be
  // cancelled.
  EXPECT_FALSE(response_client.complete());
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, CloseClient_SubsequentRequest) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, nullptr);

  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr));

  response_client.CloseBinding();
  run_loop.RunUntilIdle();

  // Not using a hanging resolver, so could be incomplete or OK depending on
  // timing of the cancellation.
  EXPECT_TRUE(!response_client.complete() ||
              response_client.result_error() == net::OK);
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Subsequent requests should be unaffected by the cancellation.
  base::RunLoop run_loop2;
  mojom::ResolveHostClientPtr response_client_ptr2;
  TestResolveHostClient response_client2(&response_client_ptr2, &run_loop2);
  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr2));
  run_loop2.Run();

  EXPECT_EQ(net::OK, response_client2.result_error());
  EXPECT_THAT(
      response_client2.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, Binding) {
  mojom::HostResolverPtr resolver_ptr;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(mojo::MakeRequest(&resolver_ptr),
                        std::move(shutdown_callback), inner_resolver.get(),
                        &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver_ptr->ResolveHost(net::HostPortPair("localhost", 160),
                            std::move(optional_parameters),
                            std::move(response_client_ptr));
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
  mojom::HostResolverPtr resolver_ptr;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  // Use a HangingHostResolver, so the test can ensure the request won't be
  // completed before the cancellation arrives.
  auto inner_resolver = std::make_unique<net::HangingHostResolver>();
  net::NetLog net_log;

  HostResolver resolver(mojo::MakeRequest(&resolver_ptr),
                        std::move(shutdown_callback), inner_resolver.get(),
                        &net_log);

  ASSERT_EQ(0, inner_resolver->num_cancellations());

  base::RunLoop run_loop;
  mojom::ResolveHostHandlePtr control_handle;
  mojom::ResolveHostParametersPtr optional_parameters =
      mojom::ResolveHostParameters::New();
  optional_parameters->control_handle = mojo::MakeRequest(&control_handle);
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);
  resolver_ptr->ResolveHost(net::HostPortPair("localhost", 160),
                            std::move(optional_parameters),
                            std::move(response_client_ptr));
  bool control_handle_closed = false;
  auto connection_error_callback =
      base::BindLambdaForTesting([&]() { control_handle_closed = true; });
  control_handle.set_connection_error_handler(connection_error_callback);

  resolver_ptr = nullptr;
  run_loop.Run();

  // Request should be cancelled.
  EXPECT_EQ(net::ERR_FAILED, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_TRUE(control_handle_closed);
  EXPECT_EQ(1, inner_resolver->num_cancellations());
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());

  // Callback should have been called.
  EXPECT_EQ(&resolver, shutdown_resolver);
}

TEST_F(HostResolverTest, CloseBinding_SubsequentRequest) {
  mojom::HostResolverPtr resolver_ptr;
  HostResolver* shutdown_resolver = nullptr;
  HostResolver::ConnectionShutdownCallback shutdown_callback =
      base::BindLambdaForTesting(
          [&](HostResolver* resolver) { shutdown_resolver = resolver; });

  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(mojo::MakeRequest(&resolver_ptr),
                        std::move(shutdown_callback), inner_resolver.get(),
                        &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, nullptr);
  resolver_ptr->ResolveHost(net::HostPortPair("localhost", 160), nullptr,
                            std::move(response_client_ptr));

  resolver_ptr = nullptr;
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
  mojom::ResolveHostClientPtr response_client_ptr2;
  TestResolveHostClient response_client2(&response_client_ptr2, &run_loop2);
  resolver.ResolveHost(net::HostPortPair("localhost", 80), nullptr,
                       std::move(response_client_ptr2));
  run_loop2.Run();

  EXPECT_EQ(net::OK, response_client2.result_error());
  EXPECT_THAT(
      response_client2.result_addresses().value().endpoints(),
      testing::UnorderedElementsAre(CreateExpectedEndPoint("127.0.0.1", 80),
                                    CreateExpectedEndPoint("::1", 80)));
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

TEST_F(HostResolverTest, IsSpeculative) {
  net::NetLog net_log;
  std::unique_ptr<net::HostResolver> inner_resolver =
      net::HostResolver::CreateDefaultResolver(&net_log);

  HostResolver resolver(inner_resolver.get(), &net_log);

  base::RunLoop run_loop;
  mojom::ResolveHostClientPtr response_client_ptr;
  TestResolveHostClient response_client(&response_client_ptr, &run_loop);
  mojom::ResolveHostParametersPtr parameters =
      mojom::ResolveHostParameters::New();
  parameters->is_speculative = true;

  // Resolve "localhost" because it should always resolve fast and locally, even
  // when using a real HostResolver.
  resolver.ResolveHost(net::HostPortPair("localhost", 80),
                       std::move(parameters), std::move(response_client_ptr));
  run_loop.Run();

  EXPECT_EQ(net::OK, response_client.result_error());
  EXPECT_FALSE(response_client.result_addresses());
  EXPECT_EQ(0u, resolver.GetNumOutstandingRequestsForTesting());
}

}  // namespace
}  // namespace network
