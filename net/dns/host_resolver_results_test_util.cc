// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_results_test_util.h"

#include <ostream>
#include <utility>
#include <vector>

#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/host_resolver_results.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class EndpointResultMatcher
    : public testing::MatcherInterface<const HostResolverEndpointResult&> {
 public:
  EndpointResultMatcher(
      testing::Matcher<std::vector<IPEndPoint>> ip_endpoints_matcher,
      testing::Matcher<const ConnectionEndpointMetadata&> metadata_matcher)
      : ip_endpoints_matcher_(std::move(ip_endpoints_matcher)),
        metadata_matcher_(std::move(metadata_matcher)) {}

  ~EndpointResultMatcher() override = default;

  EndpointResultMatcher(const EndpointResultMatcher&) = default;
  EndpointResultMatcher& operator=(const EndpointResultMatcher&) = default;
  EndpointResultMatcher(EndpointResultMatcher&&) = default;
  EndpointResultMatcher& operator=(EndpointResultMatcher&&) = default;

  bool MatchAndExplain(
      const HostResolverEndpointResult& endpoint,
      testing::MatchResultListener* result_listener) const override {
    return ExplainMatchResult(
               testing::Field("ip_endpoints",
                              &HostResolverEndpointResult::ip_endpoints,
                              ip_endpoints_matcher_),
               endpoint, result_listener) &&
           ExplainMatchResult(
               testing::Field("metadata", &HostResolverEndpointResult::metadata,
                              metadata_matcher_),
               endpoint, result_listener);
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "matches ";
    Describe(*os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not match ";
    Describe(*os);
  }

 private:
  void Describe(std::ostream& os) const {
    os << "HostResolverEndpointResult {\nip_endpoints: "
       << testing::PrintToString(ip_endpoints_matcher_)
       << "\nmetadata: " << testing::PrintToString(metadata_matcher_) << "\n}";
  }

  testing::Matcher<std::vector<IPEndPoint>> ip_endpoints_matcher_;
  testing::Matcher<const ConnectionEndpointMetadata&> metadata_matcher_;
};

}  // namespace

testing::Matcher<const HostResolverEndpointResult&> ExpectEndpointResult(
    testing::Matcher<std::vector<IPEndPoint>> ip_endpoints_matcher,
    testing::Matcher<const ConnectionEndpointMetadata&> metadata_matcher) {
  return testing::MakeMatcher(new EndpointResultMatcher(
      std::move(ip_endpoints_matcher), std::move(metadata_matcher)));
}

std::ostream& operator<<(std::ostream& os,
                         const HostResolverEndpointResult& endpoint_result) {
  return os << "HostResolverEndpointResult {\nip_endpoints: "
            << testing::PrintToString(endpoint_result.ip_endpoints)
            << "\nmetadata: "
            << testing::PrintToString(endpoint_result.metadata) << "\n}";
}

}  // namespace net
