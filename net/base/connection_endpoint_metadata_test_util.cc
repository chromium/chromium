// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/connection_endpoint_metadata_test_util.h"

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "net/base/connection_endpoint_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using EchConfigList = ConnectionEndpointMetadata::EchConfigList;

namespace {

class EndpointMetadataMatcher
    : public testing::MatcherInterface<const ConnectionEndpointMetadata&> {
 public:
  EndpointMetadataMatcher(
      testing::Matcher<std::vector<std::string>>
          supported_protocol_alpns_matcher,
      testing::Matcher<EchConfigList> ech_config_list_matcher,
      testing::Matcher<std::string> target_name_matcher)
      : supported_protocol_alpns_matcher_(
            std::move(supported_protocol_alpns_matcher)),
        ech_config_list_matcher_(std::move(ech_config_list_matcher)),
        target_name_matcher_(std::move(target_name_matcher)) {}

  ~EndpointMetadataMatcher() override = default;

  EndpointMetadataMatcher(const EndpointMetadataMatcher&) = default;
  EndpointMetadataMatcher& operator=(const EndpointMetadataMatcher&) = default;
  EndpointMetadataMatcher(EndpointMetadataMatcher&&) = default;
  EndpointMetadataMatcher& operator=(EndpointMetadataMatcher&&) = default;

  bool MatchAndExplain(
      const ConnectionEndpointMetadata& metadata,
      testing::MatchResultListener* result_listener) const override {
    return ExplainMatchResult(
               testing::Field(
                   "supported_protocol_alpns",
                   &ConnectionEndpointMetadata::supported_protocol_alpns,
                   supported_protocol_alpns_matcher_),
               metadata, result_listener) &&
           ExplainMatchResult(
               testing::Field("ech_config_list",
                              &ConnectionEndpointMetadata::ech_config_list,
                              ech_config_list_matcher_),
               metadata, result_listener) &&
           ExplainMatchResult(
               testing::Field("target_name",
                              &ConnectionEndpointMetadata::target_name,
                              target_name_matcher_),
               metadata, result_listener);
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
    os << "ConnectionEndpoint {\nsupported_protocol_alpns: "
       << testing::PrintToString(supported_protocol_alpns_matcher_)
       << "\nech_config_list: "
       << testing::PrintToString(ech_config_list_matcher_)
       << "\ntarget_name: " << testing::PrintToString(target_name_matcher_)
       << "\n}";
  }

  testing::Matcher<std::vector<std::string>> supported_protocol_alpns_matcher_;
  testing::Matcher<EchConfigList> ech_config_list_matcher_;
  testing::Matcher<std::string> target_name_matcher_;
};

}  // namespace

testing::Matcher<const ConnectionEndpointMetadata&>
ExpectConnectionEndpointMetadata(
    testing::Matcher<std::vector<std::string>> supported_protocol_alpns_matcher,
    testing::Matcher<EchConfigList> ech_config_list_matcher,
    testing::Matcher<std::string> target_name_matcher) {
  return testing::MakeMatcher(new EndpointMetadataMatcher(
      std::move(supported_protocol_alpns_matcher),
      std::move(ech_config_list_matcher), std::move(target_name_matcher)));
}

std::ostream& operator<<(
    std::ostream& os,
    const ConnectionEndpointMetadata& connection_endpoint_metadata) {
  return os << "ConnectionEndpointMetadata {\nsupported_protocol_alpns: "
            << testing::PrintToString(
                   connection_endpoint_metadata.supported_protocol_alpns)
            << "\nech_config_list: "
            << testing::PrintToString(
                   connection_endpoint_metadata.ech_config_list)
            << "\ntarget_name: "
            << testing::PrintToString(connection_endpoint_metadata.target_name)
            << "\n}";
}

}  // namespace net
