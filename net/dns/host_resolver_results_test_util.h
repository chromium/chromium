// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_RESULTS_TEST_UTIL_H_
#define NET_DNS_HOST_RESOLVER_RESULTS_TEST_UTIL_H_

#include <ostream>
#include <vector>

#include "net/base/connection_endpoint_metadata_test_util.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

struct ConnectionEndpointMetadata;
struct HostResolverEndpointResult;

testing::Matcher<const HostResolverEndpointResult&> ExpectEndpointResult(
    testing::Matcher<std::vector<IPEndPoint>> ip_endpoints_matcher =
        testing::IsEmpty(),
    testing::Matcher<const ConnectionEndpointMetadata&> metadata_matcher =
        ExpectConnectionEndpointMetadata());

std::ostream& operator<<(std::ostream& os,
                         const HostResolverEndpointResult& endpoint_result);

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_RESULTS_TEST_UTIL_H_
