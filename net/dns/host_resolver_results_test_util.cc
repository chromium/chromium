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

class ServiceEndpointMatcher
    : public testing::MatcherInterface<const ServiceEndpoint&> {
 public:
  ServiceEndpointMatcher(
      testing::Matcher<std::vector<IPEndPoint>> ipv4_endpoints_matcher,
      testing::Matcher<std::vector<IPEndPoint>> ipv6_endpoints_matcher,
      testing::Matcher<const ConnectionEndpointMetadata&> metadata_matcher)
      : ipv4_endpoints_matcher_(std::move(ipv4_endpoints_matcher)),
        ipv6_endpoints_matcher_(std::move(ipv6_endpoints_matcher)),
        metadata_matcher_(std::move(metadata_matcher)) {}

  ~ServiceEndpointMatcher() override = default;

  ServiceEndpointMatcher(const ServiceEndpointMatcher&) = default;
  ServiceEndpointMatcher& operator=(const ServiceEndpointMatcher&) = default;
  ServiceEndpointMatcher(ServiceEndpointMatcher&&) = default;
  ServiceEndpointMatcher& operator=(ServiceEndpointMatcher&&) = default;

  bool MatchAndExplain(
      const ServiceEndpoint& endpoint,
      testing::MatchResultListener* result_listener) const override {
    return ExplainMatchResult(testing::Field("ipv4_endpoints",
                                             &ServiceEndpoint::ipv4_endpoints,
                                             ipv4_endpoints_matcher_),
                              endpoint, result_listener) &&
           ExplainMatchResult(testing::Field("ipv6_endpoints",
                                             &ServiceEndpoint::ipv6_endpoints,
                                             ipv6_endpoints_matcher_),
                              endpoint, result_listener) &&
           ExplainMatchResult(
               testing::Field("metadata", &ServiceEndpoint::metadata,
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
    os << "ServiceEndpoint {\nipv4_endpoints: "
       << testing::PrintToString(ipv4_endpoints_matcher_)
       << "\npv6_endpoints: " << testing::PrintToString(ipv6_endpoints_matcher_)
       << "\nmetadata: " << testing::PrintToString(metadata_matcher_) << "\n}";
  }

  testing::Matcher<std::vector<IPEndPoint>> ipv4_endpoints_matcher_;
  testing::Matcher<std::vector<IPEndPoint>> ipv6_endpoints_matcher_;
  testing::Matcher<const ConnectionEndpointMetadata&> metadata_matcher_;
};

}  // namespace

testing::Matcher<const HostResolverEndpointResult&> ExpectEndpointResult(
    testing::Matcher<std::vector<IPEndPoint>> ip_endpoints_matcher,
    testing::Matcher<const ConnectionEndpointMetadata&> metadata_matcher) {
  return testing::MakeMatcher(new EndpointResultMatcher(
      std::move(ip_endpoints_matcher), std::move(metadata_matcher)));
}

testing::Matcher<const ServiceEndpoint&> ExpectServiceEndpoint(
    testing::Matcher<std::vector<IPEndPoint>> ipv4_endpoints_matcher,
    testing::Matcher<std::vector<IPEndPoint>> ipv6_endpoints_matcher,
    testing::Matcher<const ConnectionEndpointMetadata&> metadata_matcher) {
  return testing::MakeMatcher(new ServiceEndpointMatcher(
      std::move(ipv4_endpoints_matcher), std::move(ipv6_endpoints_matcher),
      std::move(metadata_matcher)));
}

std::ostream& operator<<(std::ostream& os,
                         const HostResolverEndpointResult& endpoint_result) {
  return os << "HostResolverEndpointResult {\nip_endpoints: "
            << testing::PrintToString(endpoint_result.ip_endpoints)
            << "\nmetadata: "
            << testing::PrintToString(endpoint_result.metadata) << "\n}";
}

std::ostream& operator<<(std::ostream& os, const ServiceEndpoint& endpoint) {
  return os << "ServiceEndpoint {\nipv4_endpoints: "
            << testing::PrintToString(endpoint.ipv4_endpoints)
            << "\nipv6_endpoints: "
            << testing::PrintToString(endpoint.ipv6_endpoints)
            << "\nmetadata: " << testing::PrintToString(endpoint.metadata)
            << "\n}";
}

}  // namespace net
