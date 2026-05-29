// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

static IPEndPoint MakeIPEndPoint(std::string_view addr, int port) {
  return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
}

using MappedHostResolverTest = TestWithTaskEnvironment;

std::string FirstAddress(const AddressList& address_list) {
  if (address_list.empty())
    return std::string();
  return address_list.front().ToString();
}

TEST_F(MappedHostResolverTest, Inclusion) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddSimulatedFailure("*google.com");
  resolver_impl->rules()->AddRule("baz.com", "192.168.1.5");
  resolver_impl->rules()->AddRule("foo.com", "192.168.1.8");
  resolver_impl->rules()->AddRule("proxy", "192.168.1.11");

  // Create a remapped resolver that uses |resolver_impl|.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));

  // Try resolving "www.google.com:80". There are no mappings yet, so this
  // hits |resolver_impl| and fails.
  TestCompletionCallback callback;
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("www.google.com", 80),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);
  int rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsError(ERR_NAME_NOT_RESOLVED));

  // Remap *.google.com to baz.com.
  EXPECT_TRUE(resolver->AddRuleFromString("map *.google.com baz.com"));
  request.reset();

  // Try resolving "www.google.com:80". Should be remapped to "baz.com:80".
  request = resolver->CreateRequest(HostPortPair("www.google.com", 80),
                                    NetworkAnonymizationKey(),
                                    NetLogWithSource(), std::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.5:80", FirstAddress(request->GetAddressResults()));
  request.reset();

  // Try resolving "foo.com:77". This will NOT be remapped, so result
  // is "foo.com:77".
  request = resolver->CreateRequest(HostPortPair("foo.com", 77),
                                    NetworkAnonymizationKey(),
                                    NetLogWithSource(), std::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.8:77", FirstAddress(request->GetAddressResults()));
  request.reset();

  // Remap "*.org" to "proxy:99".
  EXPECT_TRUE(resolver->AddRuleFromString("Map *.org proxy:99"));

  // Try resolving "chromium.org:61". Should be remapped to "proxy:99".
  request = resolver->CreateRequest(HostPortPair("chromium.org", 61),
                                    NetworkAnonymizationKey(),
                                    NetLogWithSource(), std::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.11:99", FirstAddress(request->GetAddressResults()));
}

TEST_F(MappedHostResolverTest, MapsHostWithScheme) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("remapped.test", "192.168.1.22");

  // Create a remapped resolver that uses `resolver_impl`.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));
  ASSERT_TRUE(resolver->AddRuleFromString("MAP to.map.test remapped.test"));

  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(
          url::SchemeHostPort(url::kHttpScheme, "to.map.test", 155),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_THAT(
      request->GetAddressResults(),
      testing::ElementsAre(IPEndPoint(IPAddress(192, 168, 1, 22), 155)));
}

TEST_F(MappedHostResolverTest, MapsHostWithSchemeToIpLiteral) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("host.test", "192.168.1.22");

  // Create a remapped resolver that uses `resolver_impl`.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));
  ASSERT_TRUE(resolver->AddRuleFromString("MAP host.test [1234:5678::000A]"));

  IPAddress expected_address;
  ASSERT_TRUE(expected_address.AssignFromIPLiteral("1234:5678::000A"));

  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(
          url::SchemeHostPort(url::kHttpScheme, "host.test", 156),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_THAT(request->GetAddressResults(),
              testing::ElementsAre(IPEndPoint(expected_address, 156)));
}

// Tests that remapped URL gets canonicalized when passing scheme.
TEST_F(MappedHostResolverTest, MapsHostWithSchemeToNonCanon) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("remapped.test", "192.168.1.23");

  // Create a remapped resolver that uses `resolver_impl`.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));
  ASSERT_TRUE(resolver->AddRuleFromString("MAP host.test reMapped.TEST"));

  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(
          url::SchemeHostPort(url::kHttpScheme, "host.test", 157),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_THAT(
      request->GetAddressResults(),
      testing::ElementsAre(IPEndPoint(IPAddress(192, 168, 1, 23), 157)));
}

TEST_F(MappedHostResolverTest, MapsHostWithSchemeToNameWithPort) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("remapped.test", "192.168.1.24");

  // Create a remapped resolver that uses `resolver_impl`.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));
  ASSERT_TRUE(resolver->AddRuleFromString("MAP host.test remapped.test:258"));

  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(
          url::SchemeHostPort(url::kHttpScheme, "host.test", 158),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_THAT(
      request->GetAddressResults(),
      testing::ElementsAre(IPEndPoint(IPAddress(192, 168, 1, 24), 258)));
}

TEST_F(MappedHostResolverTest, HandlesUnmappedHostWithScheme) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("unmapped.test", "192.168.1.23");

  // Create a remapped resolver that uses `resolver_impl`.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));

  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(
          url::SchemeHostPort(url::kHttpsScheme, "unmapped.test", 155),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());

  EXPECT_THAT(callback.GetResult(rv), IsOk());
  EXPECT_THAT(
      request->GetAddressResults(),
      testing::ElementsAre(IPEndPoint(IPAddress(192, 168, 1, 23), 155)));
}

// Tests that exclusions are respected.
TEST_F(MappedHostResolverTest, Exclusion) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("baz", "192.168.1.5");
  resolver_impl->rules()->AddRule("www.google.com", "192.168.1.3");

  // Create a remapped resolver that uses |resolver_impl|.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));

  TestCompletionCallback callback;

  // Remap "*.com" to "baz".
  EXPECT_TRUE(resolver->AddRuleFromString("map *.com baz"));

  // Add an exclusion for "*.google.com".
  EXPECT_TRUE(resolver->AddRuleFromString("EXCLUDE *.google.com"));

  // Try resolving "www.google.com". Should not be remapped due to exclusion).
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("www.google.com", 80),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);
  int rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.3:80", FirstAddress(request->GetAddressResults()));
  request.reset();

  // Try resolving "chrome.com:80". Should be remapped to "baz:80".
  request = resolver->CreateRequest(HostPortPair("chrome.com", 80),
                                    NetworkAnonymizationKey(),
                                    NetLogWithSource(), std::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.5:80", FirstAddress(request->GetAddressResults()));
}

TEST_F(MappedHostResolverTest, SetRulesFromString) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("baz", "192.168.1.7");
  resolver_impl->rules()->AddRule("bar", "192.168.1.9");

  // Create a remapped resolver that uses |resolver_impl|.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));

  TestCompletionCallback callback;

  // Remap "*.com" to "baz", and *.net to "bar:60".
  resolver->SetRulesFromString("map *.com baz , map *.net bar:60");

  // Try resolving "www.google.com". Should be remapped to "baz".
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("www.google.com", 80),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);
  int rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.7:80", FirstAddress(request->GetAddressResults()));
  request.reset();

  // Try resolving "chrome.net:80". Should be remapped to "bar:60".
  request = resolver->CreateRequest(HostPortPair("chrome.net", 80),
                                    NetworkAnonymizationKey(),
                                    NetLogWithSource(), std::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.9:60", FirstAddress(request->GetAddressResults()));
}

// Parsing bad rules should silently discard the rule (and never crash).
TEST_F(MappedHostResolverTest, ParseInvalidRules) {
  auto resolver =
      std::make_unique<MappedHostResolver>(std::unique_ptr<HostResolver>());

  EXPECT_FALSE(resolver->AddRuleFromString("xyz"));
  EXPECT_FALSE(resolver->AddRuleFromString(std::string()));
  EXPECT_FALSE(resolver->AddRuleFromString(" "));
  EXPECT_FALSE(resolver->AddRuleFromString("EXCLUDE"));
  EXPECT_FALSE(resolver->AddRuleFromString("EXCLUDE foo bar"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE x"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE x :10"));
}

// Test mapping hostnames to resolving failures.
TEST_F(MappedHostResolverTest, MapToError) {
  // Outstanding request.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("*", "192.168.1.5");

  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));

  // Remap *.google.com to resolving failures.
  EXPECT_TRUE(resolver->AddRuleFromString("MAP *.google.com ^NOTFOUND"));

  // Try resolving www.google.com --> Should give an error.
  TestCompletionCallback callback1;
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("www.google.com", 80),
                              NetworkAnonymizationKey(), NetLogWithSource(),
                              std::nullopt);
  int rv = request->Start(callback1.callback());
  EXPECT_THAT(rv, IsError(ERR_NAME_NOT_RESOLVED));
  request.reset();

  // Try resolving www.foo.com --> Should succeed.
  TestCompletionCallback callback2;
  request = resolver->CreateRequest(HostPortPair("www.foo.com", 80),
                                    NetworkAnonymizationKey(),
                                    NetLogWithSource(), std::nullopt);
  rv = request->Start(callback2.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback2.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.5:80", FirstAddress(request->GetAddressResults()));
}

TEST_F(MappedHostResolverTest, MapHostWithSchemeToError) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("host.test", "192.168.1.25");

  // Create a remapped resolver that uses `resolver_impl`.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));
  ASSERT_TRUE(resolver->AddRuleFromString("MAP host.test ^NOTFOUND"));

  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(
          url::SchemeHostPort(url::kWssScheme, "host.test", 155),
          NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt);

  TestCompletionCallback callback;
  int rv = request->Start(callback.callback());
  EXPECT_THAT(callback.GetResult(rv), IsError(ERR_NAME_NOT_RESOLVED));
}

class TestServiceEndpointRequestDelegate final
    : public HostResolver::ServiceEndpointRequest::Delegate {
 public:
  explicit TestServiceEndpointRequestDelegate(
      std::unique_ptr<HostResolver::ServiceEndpointRequest> request)
      : request_(std::move(request)) {}

  ~TestServiceEndpointRequestDelegate() override = default;

  // Starts `request_` and waits for it to complete, if needed.
  int StartAndWaitForResult() {
    int rv = request_->Start(this);
    if (rv != ERR_IO_PENDING) {
      // This is to catch the case that OnServiceEndpointRequestFinished() is
      // invoked, too.
      OnServiceEndpointRequestFinished(rv);
      return rv;
    } else {
      run_loop_.Run();
    }
    CHECK(result_);
    return *result_;
  }

  // HostResolver::ServiceEndpointRequest::Delegate implementation:

  void OnServiceEndpointsUpdated() override {}

  void OnServiceEndpointRequestFinished(int rv) override {
    CHECK(!result_);
    result_ = rv;
    run_loop_.Quit();
  }

  HostResolver::ServiceEndpointRequest& request() { return *request_; }

 private:
  std::unique_ptr<HostResolver::ServiceEndpointRequest> request_;
  base::RunLoop run_loop_;
  std::optional<int> result_;
};

// Creates and runs a ServiceEndpointRequest for `host` expecting
// `expected_result`. If `skip_host_lookup_check` is true, the last host passed
// to the MockHostResolver will not be checked for consistency - this is useful
// when the MappedHostResolver is expected  to fail a request without sending it
// to the MockHostResolver.
void RunServiceEndpointTestForHost(
    MappedHostResolver& resolver,
    MockHostResolver& mock_resolver_ptr,
    const HostResolver::Host& host,
    const base::expected<IPEndPoint, Error>& expected_result,
    bool skip_host_lookup_check = false) {
  std::unique_ptr<HostResolver::ServiceEndpointRequest> request =
      resolver.CreateServiceEndpointRequest(
          host, NetworkAnonymizationKey(), NetLogWithSource(),
          HostResolver::ResolveHostParameters());

  TestServiceEndpointRequestDelegate delegate(std::move(request));
  int rv = delegate.StartAndWaitForResult();

  // Check that scheme is preserved. The MockResolver currently ignores it, so
  // can't make result vary based on it.
  const std::optional<HostResolver::Host>& request_host =
      mock_resolver_ptr.last_observed_host();
  CHECK(request_host);

  if (!skip_host_lookup_check) {
    EXPECT_EQ(request_host->HasScheme(), host.HasScheme());
    if (request_host->HasScheme() && host.HasScheme()) {
      EXPECT_EQ(request_host->GetScheme(), host.GetScheme());
    }
  }

  if (!expected_result.has_value()) {
    EXPECT_THAT(rv, IsError(expected_result.error()));
    return;
  }

  ASSERT_THAT(rv, IsOk());
  EXPECT_EQ(
      delegate.request().GetEndpointResults(),
      std::vector<ServiceEndpoint>{ServiceEndpointBuilder()
                                       .add_ip_endpoint(expected_result.value())
                                       .endpoint()});
}

// Creates and runs two ServiceEndpointRequests for `scheme_host_port`: One with
// the scheme included, one without it. Expects both to return
// `expected_result`. Also runs a pair of ResolveHostRequest requests, again
// with both input types, expecting the same result to check for consistency.
void RunServiceEndpointTests(
    MappedHostResolver& resolver,
    MockHostResolver& mock_resolver_ptr,
    url::SchemeHostPort scheme_host_port,
    const base::expected<IPEndPoint, Error>& expected_result,
    bool skip_host_lookup_check = false) {
  auto host_port_pair = HostPortPair::FromSchemeHostPort(scheme_host_port);

  RunServiceEndpointTestForHost(resolver, mock_resolver_ptr,
                                HostResolver::Host(scheme_host_port),
                                expected_result, skip_host_lookup_check);
  RunServiceEndpointTestForHost(resolver, mock_resolver_ptr,
                                HostResolver::Host(host_port_pair),
                                expected_result, skip_host_lookup_check);

  for (bool resolve_with_scheme : {false, true}) {
    std::unique_ptr<HostResolver::ResolveHostRequest> request;
    if (resolve_with_scheme) {
      request =
          resolver.CreateRequest(scheme_host_port, NetworkAnonymizationKey(),
                                 NetLogWithSource(), std::nullopt);
    } else {
      request =
          resolver.CreateRequest(host_port_pair, NetworkAnonymizationKey(),
                                 NetLogWithSource(), std::nullopt);
    }

    TestCompletionCallback callback;
    int rv = request->Start(callback.callback());
    if (!expected_result.has_value()) {
      EXPECT_THAT(callback.GetResult(rv), expected_result.error());
    } else {
      EXPECT_THAT(callback.GetResult(rv), IsOk());
      EXPECT_THAT(request->GetAddressResults(),
                  testing::ElementsAre(expected_result.value()));
    }
  }
}

TEST_F(MappedHostResolverTest, ServiceEndpointRequest) {
  auto resolver_impl = std::make_unique<MockHostResolver>();
  resolver_impl->rules()->AddRule("good.test", "192.168.1.25");
  resolver_impl->rules()->AddSimulatedFailure("bad0.test");
  MockHostResolver* mock_resolver_ptr = resolver_impl.get();

  // Create a remapped resolver that uses `resolver_impl`.
  auto resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver_impl));
  ASSERT_TRUE(resolver->AddRuleFromString("Map a.test 1.2.3.4"));
  ASSERT_TRUE(resolver->AddRuleFromString("Map b.test [1234:5678::000A]"));
  ASSERT_TRUE(resolver->AddRuleFromString("Map c.test 2.3.4.5:67"));
  ASSERT_TRUE(resolver->AddRuleFromString("Map *.d.test good.test:99"));
  ASSERT_TRUE(resolver->AddRuleFromString("Map bad1.test bad0.test:1234"));
  ASSERT_TRUE(resolver->AddRuleFromString("Map bad2.test ^NOTFOUND"));

  RunServiceEndpointTests(
      *resolver, *mock_resolver_ptr,
      url::SchemeHostPort(url::kHttpScheme, "good.test", 155),
      MakeIPEndPoint("192.168.1.25", 155));
  RunServiceEndpointTests(*resolver, *mock_resolver_ptr,
                          url::SchemeHostPort(url::kHttpScheme, "a.test", 155),
                          MakeIPEndPoint("1.2.3.4", 155));
  RunServiceEndpointTests(*resolver, *mock_resolver_ptr,
                          url::SchemeHostPort(url::kHttpsScheme, "a.test", 155),
                          MakeIPEndPoint("1.2.3.4", 155));
  RunServiceEndpointTests(*resolver, *mock_resolver_ptr,
                          url::SchemeHostPort(url::kHttpScheme, "b.test", 155),
                          MakeIPEndPoint("1234:5678::a", 155));
  RunServiceEndpointTests(*resolver, *mock_resolver_ptr,
                          url::SchemeHostPort(url::kHttpScheme, "c.test", 155),
                          MakeIPEndPoint("2.3.4.5", 67));
  RunServiceEndpointTests(
      *resolver, *mock_resolver_ptr,
      url::SchemeHostPort(url::kHttpsScheme, "a.d.test", 443),
      MakeIPEndPoint("192.168.1.25", 99));
  RunServiceEndpointTests(
      *resolver, *mock_resolver_ptr,
      url::SchemeHostPort(url::kHttpScheme, "bad0.test", 80),
      base::unexpected(ERR_NAME_NOT_RESOLVED));
  RunServiceEndpointTests(
      *resolver, *mock_resolver_ptr,
      url::SchemeHostPort(url::kHttpScheme, "bad1.test", 80),
      base::unexpected(ERR_NAME_NOT_RESOLVED));
  // Have to skip the host check in this test, because the MappedHostResolver
  // fails the request itself, rather than sending it down to the
  // MockHostResolver. In all other cases, requests are passed down to the
  // MockHostResolver, even if a host is mapped directly to an IP.
  RunServiceEndpointTests(
      *resolver, *mock_resolver_ptr,
      url::SchemeHostPort(url::kHttpScheme, "bad2.test", 81),
      base::unexpected(ERR_NAME_NOT_RESOLVED), /*skip_host_lookup_check=*/true);
}

}  // namespace

}  // namespace net
