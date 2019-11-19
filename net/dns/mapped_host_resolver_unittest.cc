// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include <utility>

#include "base/test/task_environment.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  std::unique_ptr<MockHostResolver> resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddSimulatedFailure("*google.com");
  resolver_impl->rules()->AddRule("baz.com", "192.168.1.5");
  resolver_impl->rules()->AddRule("foo.com", "192.168.1.8");
  resolver_impl->rules()->AddRule("proxy", "192.168.1.11");

  // Create a remapped resolver that uses |resolver_impl|.
  std::unique_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(std::move(resolver_impl)));

  // Try resolving "www.google.com:80". There are no mappings yet, so this
  // hits |resolver_impl| and fails.
  TestCompletionCallback callback;
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("www.google.com", 80),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);
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
                                    NetworkIsolationKey(), NetLogWithSource(),
                                    base::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.5:80",
            FirstAddress(request->GetAddressResults().value()));
  request.reset();

  // Try resolving "foo.com:77". This will NOT be remapped, so result
  // is "foo.com:77".
  request = resolver->CreateRequest(HostPortPair("foo.com", 77),
                                    NetworkIsolationKey(), NetLogWithSource(),
                                    base::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.8:77",
            FirstAddress(request->GetAddressResults().value()));
  request.reset();

  // Remap "*.org" to "proxy:99".
  EXPECT_TRUE(resolver->AddRuleFromString("Map *.org proxy:99"));

  // Try resolving "chromium.org:61". Should be remapped to "proxy:99".
  request = resolver->CreateRequest(HostPortPair("chromium.org", 61),
                                    NetworkIsolationKey(), NetLogWithSource(),
                                    base::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.11:99",
            FirstAddress(request->GetAddressResults().value()));
}

// Tests that exclusions are respected.
TEST(MappedHostResolverTest, Exclusion) {
  base::test::TaskEnvironment task_environment;

  // Create a mock host resolver, with specific hostname to IP mappings.
  std::unique_ptr<MockHostResolver> resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddRule("baz", "192.168.1.5");
  resolver_impl->rules()->AddRule("www.google.com", "192.168.1.3");

  // Create a remapped resolver that uses |resolver_impl|.
  std::unique_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(std::move(resolver_impl)));

  TestCompletionCallback callback;

  // Remap "*.com" to "baz".
  EXPECT_TRUE(resolver->AddRuleFromString("map *.com baz"));

  // Add an exclusion for "*.google.com".
  EXPECT_TRUE(resolver->AddRuleFromString("EXCLUDE *.google.com"));

  // Try resolving "www.google.com". Should not be remapped due to exclusion).
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("www.google.com", 80),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);
  int rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.3:80",
            FirstAddress(request->GetAddressResults().value()));
  request.reset();

  // Try resolving "chrome.com:80". Should be remapped to "baz:80".
  request = resolver->CreateRequest(HostPortPair("chrome.com", 80),
                                    NetworkIsolationKey(), NetLogWithSource(),
                                    base::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.5:80",
            FirstAddress(request->GetAddressResults().value()));
}

TEST(MappedHostResolverTest, SetRulesFromString) {
  base::test::TaskEnvironment task_environment;

  // Create a mock host resolver, with specific hostname to IP mappings.
  std::unique_ptr<MockHostResolver> resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddRule("baz", "192.168.1.7");
  resolver_impl->rules()->AddRule("bar", "192.168.1.9");

  // Create a remapped resolver that uses |resolver_impl|.
  std::unique_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(std::move(resolver_impl)));

  TestCompletionCallback callback;

  // Remap "*.com" to "baz", and *.net to "bar:60".
  resolver->SetRulesFromString("map *.com baz , map *.net bar:60");

  // Try resolving "www.google.com". Should be remapped to "baz".
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("www.google.com", 80),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);
  int rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.7:80",
            FirstAddress(request->GetAddressResults().value()));
  request.reset();

  // Try resolving "chrome.net:80". Should be remapped to "bar:60".
  request = resolver->CreateRequest(HostPortPair("chrome.net", 80),
                                    NetworkIsolationKey(), NetLogWithSource(),
                                    base::nullopt);
  rv = request->Start(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.9:60",
            FirstAddress(request->GetAddressResults().value()));
}

// Parsing bad rules should silently discard the rule (and never crash).
TEST(MappedHostResolverTest, ParseInvalidRules) {
  base::test::TaskEnvironment task_environment;

  std::unique_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(std::unique_ptr<HostResolver>()));

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
  std::unique_ptr<MockHostResolver> resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddRule("*", "192.168.1.5");

  std::unique_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(std::move(resolver_impl)));

  // Remap *.google.com to resolving failures.
  EXPECT_TRUE(resolver->AddRuleFromString("MAP *.google.com ~NOTFOUND"));

  // Try resolving www.google.com --> Should give an error.
  TestCompletionCallback callback1;
  std::unique_ptr<HostResolver::ResolveHostRequest> request =
      resolver->CreateRequest(HostPortPair("www.google.com", 80),
                              NetworkIsolationKey(), NetLogWithSource(),
                              base::nullopt);
  int rv = request->Start(callback1.callback());
  EXPECT_THAT(rv, IsError(ERR_NAME_NOT_RESOLVED));
  request.reset();

  // Try resolving www.foo.com --> Should succeed.
  TestCompletionCallback callback2;
  request = resolver->CreateRequest(HostPortPair("www.foo.com", 80),
                                    NetworkIsolationKey(), NetLogWithSource(),
                                    base::nullopt);
  rv = request->Start(callback2.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback2.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ("192.168.1.5:80",
            FirstAddress(request->GetAddressResults().value()));
}

}  // namespace

}  // namespace net
