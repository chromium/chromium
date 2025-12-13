// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note that most functionality of HostResolverSystemTask is tested indirectly
// via the HostResolverManager tests.

#include "net/dns/host_resolver_system_task.h"

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver_internal_result_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

namespace net {
namespace {

TEST(HostResolverSystemTaskTest, ConvertResults) {
  const IPEndPoint kExpectedIpv4(IPAddress(192, 0, 2, 45), 51123);
  const IPEndPoint kExpectedIpv6(
      IPAddress::FromIPLiteral("2001:db8::45").value(), 51124);
  constexpr std::string_view kExpectedDomain = "domain.test";

  constexpr base::Time kNow;
  constexpr base::TimeTicks kNowTicks;

  AddressList address_list;
  address_list.push_back(kExpectedIpv4);
  address_list.push_back(kExpectedIpv6);

  std::set<std::unique_ptr<HostResolverInternalResult>> results =
      HostResolverSystemTask::ConvertSystemResults(
          kExpectedDomain, {DnsQueryType::A, DnsQueryType::AAAA}, address_list,
          kNow, kNowTicks);

  EXPECT_THAT(results,
              UnorderedElementsAre(
                  Pointee(ExpectHostResolverInternalDataResult(
                      std::string(kExpectedDomain), DnsQueryType::A,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      ElementsAre(kExpectedIpv4))),
                  Pointee(ExpectHostResolverInternalDataResult(
                      std::string(kExpectedDomain), DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      ElementsAre(kExpectedIpv6)))));
}

TEST(HostResolverSystemTaskTest, ConvertIpv4OnlyResults) {
  const IPEndPoint kExpectedIpv4(IPAddress(192, 0, 2, 45), 51123);
  constexpr std::string_view kExpectedDomain = "domain.test";

  constexpr base::Time kNow;
  constexpr base::TimeTicks kNowTicks;

  AddressList address_list;
  address_list.push_back(kExpectedIpv4);

  std::set<std::unique_ptr<HostResolverInternalResult>> results =
      HostResolverSystemTask::ConvertSystemResults(
          kExpectedDomain, {DnsQueryType::A}, address_list, kNow, kNowTicks);

  EXPECT_THAT(results,
              UnorderedElementsAre(Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kExpectedDomain), DnsQueryType::A,
                  HostResolverInternalResult::Source::kUnknown,
                  Optional(kNowTicks + HostResolverSystemTask::kTtl),
                  Optional(kNow + HostResolverSystemTask::kTtl),
                  ElementsAre(kExpectedIpv4)))));
}

TEST(HostResolverSystemTaskTest, ConvertEmptyResults) {
  constexpr std::string_view kExpectedDomain = "domain.test";

  constexpr base::Time kNow;
  constexpr base::TimeTicks kNowTicks;

  std::set<std::unique_ptr<HostResolverInternalResult>> results =
      HostResolverSystemTask::ConvertSystemResults(
          kExpectedDomain, {DnsQueryType::A, DnsQueryType::AAAA}, AddressList(),
          kNow, kNowTicks);

  EXPECT_THAT(results,
              UnorderedElementsAre(
                  Pointee(ExpectHostResolverInternalErrorResult(
                      std::string(kExpectedDomain), DnsQueryType::A,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      ERR_NAME_NOT_RESOLVED)),
                  Pointee(ExpectHostResolverInternalErrorResult(
                      std::string(kExpectedDomain), DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      ERR_NAME_NOT_RESOLVED))));
}

TEST(HostResolverSystemTaskTest, ConvertResultsWithAlias) {
  const IPEndPoint kExpectedIpv4(IPAddress(192, 0, 2, 45), 51123);
  const IPEndPoint kExpectedIpv6(
      IPAddress::FromIPLiteral("2001:db8::45").value(), 51124);
  constexpr std::string_view kExpectedDomain = "domain.test";
  constexpr std::string_view kExpectedAliasTarget = "alias.target.test";

  constexpr base::Time kNow;
  constexpr base::TimeTicks kNowTicks;

  AddressList address_list;
  address_list.push_back(kExpectedIpv4);
  address_list.push_back(kExpectedIpv6);
  address_list.SetDnsAliases({std::string(kExpectedAliasTarget)});

  std::set<std::unique_ptr<HostResolverInternalResult>> results =
      HostResolverSystemTask::ConvertSystemResults(
          kExpectedDomain, {DnsQueryType::A, DnsQueryType::AAAA}, address_list,
          kNow, kNowTicks);

  EXPECT_THAT(results,
              UnorderedElementsAre(
                  Pointee(ExpectHostResolverInternalDataResult(
                      std::string(kExpectedAliasTarget), DnsQueryType::A,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      ElementsAre(kExpectedIpv4))),
                  Pointee(ExpectHostResolverInternalDataResult(
                      std::string(kExpectedAliasTarget), DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      ElementsAre(kExpectedIpv6))),
                  Pointee(ExpectHostResolverInternalAliasResult(
                      std::string(kExpectedDomain), DnsQueryType::A,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      std::string(kExpectedAliasTarget))),
                  Pointee(ExpectHostResolverInternalAliasResult(
                      std::string(kExpectedDomain), DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      std::string(kExpectedAliasTarget)))));
}

TEST(HostResolverSystemTaskTest, ConvertIpv6OnlyResultsWithAlias) {
  const IPEndPoint kExpectedIpv6(
      IPAddress::FromIPLiteral("2001:db8::45").value(), 51124);
  constexpr std::string_view kExpectedDomain = "domain.test";
  constexpr std::string_view kExpectedAliasTarget = "alias.target.test";

  constexpr base::Time kNow;
  constexpr base::TimeTicks kNowTicks;

  AddressList address_list;
  address_list.push_back(kExpectedIpv6);
  address_list.SetDnsAliases({std::string(kExpectedAliasTarget)});

  std::set<std::unique_ptr<HostResolverInternalResult>> results =
      HostResolverSystemTask::ConvertSystemResults(
          kExpectedDomain, {DnsQueryType::AAAA}, address_list, kNow, kNowTicks);

  EXPECT_THAT(results,
              UnorderedElementsAre(
                  Pointee(ExpectHostResolverInternalDataResult(
                      std::string(kExpectedAliasTarget), DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      ElementsAre(kExpectedIpv6))),
                  Pointee(ExpectHostResolverInternalAliasResult(
                      std::string(kExpectedDomain), DnsQueryType::AAAA,
                      HostResolverInternalResult::Source::kUnknown,
                      Optional(kNowTicks + HostResolverSystemTask::kTtl),
                      Optional(kNow + HostResolverSystemTask::kTtl),
                      std::string(kExpectedAliasTarget)))));
}

}  // namespace
}  // namespace net
