// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/test/task_environment.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

std::string FirstAddress(const AddressList& address_list) {
  if (address_list.empty())
    return std::string();
  return address_list.front().ToString();
}

TEST(MappedHostResolverTest, Inclusion) {
  base::test::TaskEnvironment task_environment;

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
  EXPECT_FALSE(request->GetAddressResults());

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
  EXPECT_EQ("192.168.1.5:80", FirstAddress(*request->GetAddressResults()));
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
  EXPECT_EQ("192.168.1.8:77", FirstAddress(*request->GetAddressResults()));
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
  EXPECT_EQ("192.168.1.11:99", FirstAddress(*request->GetAddressResults()));
}

TEST(MappedHostResolverTest, MapsHostWithScheme) {
  base::test::TaskEnvironment task_environment;

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
      request->GetAddressResults()->endpoints(),
      testing::ElementsAre(IPEndPoint(IPAddress(192, 168, 1, 22), 155)));
}

TEST(MappedHostResolverTest, MapsHostWithSchemeToIpLiteral) {
  base::test::TaskEnvironment task_environment;

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
  EXPECT_THAT(request->GetAddressResults()->endpoints(),
              testing::ElementsAre(IPEndPoint(expected_address, 156)));
}

// Tests that remapped URL gets canonicalized when passing scheme.
TEST(MappedHostResolverTest, MapsHostWithSchemeToNonCanon) {
  base::test::TaskEnvironment task_environment;

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
      request->GetAddressResults()->endpoints(),
      testing::ElementsAre(IPEndPoint(IPAddress(192, 168, 1, 23), 157)));
}

TEST(MappedHostResolverTest, MapsHostWithSchemeToNameWithPort) {
  base::test::TaskEnvironment task_environment;

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
      request->GetAddressResults()->endpoints(),
      testing::ElementsAre(IPEndPoint(IPAddress(192, 168, 1, 24), 258)));
}

TEST(MappedHostResolverTest, HandlesUnmappedHostWithScheme) {
  base::test::TaskEnvironment task_environment;

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
      request->GetAddressResults()->endpoints(),
      testing::ElementsAre(IPEndPoint(IPAddress(192, 168, 1, 23), 155)));
}

// Tests that exclusions are respected.
TEST(MappedHostResolverTest, Exclusion) {
  base::test::TaskEnvironment task_environment;

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
  EXPECT_EQ("192.168.1.3:80", FirstAddress(*request->GetAddressResults()));
  request.reset();

  // Try resolving "chrome.com:80". Should be remapped to "baz:80".
  request = resolver->CreateRequest(HostPortPair("chrome.com", 80),
                                    NetworkAnonymizationKey(),
                                    NetLogWithSource(), std::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.5:80", FirstAddress(*request->GetAddressResults()));
}

TEST(MappedHostResolverTest, SetRulesFromString) {
  base::test::TaskEnvironment task_environment;

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
  EXPECT_EQ("192.168.1.7:80", FirstAddress(*request->GetAddressResults()));
  request.reset();

  // Try resolving "chrome.net:80". Should be remapped to "bar:60".
  request = resolver->CreateRequest(HostPortPair("chrome.net", 80),
                                    NetworkAnonymizationKey(),
                                    NetLogWithSource(), std::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.9:60", FirstAddress(*request->GetAddressResults()));
}

// Parsing bad rules should silently discard the rule (and never crash).
TEST(MappedHostResolverTest, ParseInvalidRules) {
  base::test::TaskEnvironment task_environment;

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
TEST(MappedHostResolverTest, MapToError) {
  base::test::TaskEnvironment task_environment;

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
  EXPECT_EQ("192.168.1.5:80", FirstAddress(*request->GetAddressResults()));
}

TEST(MappedHostResolverTest, MapHostWithSchemeToError) {
  base::test::TaskEnvironment task_environment;

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

}  // namespace

}  // namespace net
