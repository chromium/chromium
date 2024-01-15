// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_INTERNAL_RESULT_TEST_UTIL_H_
#define NET_DNS_HOST_RESOLVER_INTERNAL_RESULT_TEST_UTIL_H_

#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_query_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

testing::Matcher<const HostResolverInternalResult&>
ExpectHostResolverInternalDataResult(
    std::string expected_domain_name,
    DnsQueryType expected_query_type,
    HostResolverInternalResult::Source expected_source,
    testing::Matcher<std::optional<base::TimeTicks>> expiration_matcher,
    testing::Matcher<std::optional<base::Time>> timed_expiration_matcher,
    testing::Matcher<std::vector<IPEndPoint>> endpoints_matcher =
        testing::IsEmpty(),
    testing::Matcher<std::vector<std::string>> strings_matcher =
        testing::IsEmpty(),
    testing::Matcher<std::vector<HostPortPair>> hosts_matcher =
        testing::IsEmpty());

testing::Matcher<const HostResolverInternalResult&>
ExpectHostResolverInternalMetadataResult(
    std::string expected_domain_name,
    DnsQueryType expected_query_type,
    HostResolverInternalResult::Source expected_source,
    testing::Matcher<std::optional<base::TimeTicks>> expiration_matcher,
    testing::Matcher<std::optional<base::Time>> timed_expiration_matcher,
    testing::Matcher<
        std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>>
        metadatas_matcher = testing::IsEmpty());

testing::Matcher<const HostResolverInternalResult&>
ExpectHostResolverInternalErrorResult(
    std::string expected_domain_name,
    DnsQueryType expected_query_type,
    HostResolverInternalResult::Source expected_source,
    testing::Matcher<std::optional<base::TimeTicks>> expiration_matcher,
    testing::Matcher<std::optional<base::Time>> timed_expiration_matcher,
    int expected_error);

testing::Matcher<const HostResolverInternalResult&>
ExpectHostResolverInternalAliasResult(
    std::string expected_domain_name,
    DnsQueryType expected_query_type,
    HostResolverInternalResult::Source expected_source,
    testing::Matcher<std::optional<base::TimeTicks>> expiration_matcher,
    testing::Matcher<std::optional<base::Time>> timed_expiration_matcher,
    std::string expected_alias_target);

std::ostream& operator<<(std::ostream& os,
                         const HostResolverInternalResult& result);

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_INTERNAL_RESULT_TEST_UTIL_H_
