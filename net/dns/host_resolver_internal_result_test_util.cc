// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_internal_result_test_util.h"

#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/json/json_writer.h"
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

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatchResultListener;
using ::testing::PrintToString;
using ::testing::Property;
using ::testing::StringMatchResultListener;

namespace {

class HostResolverInternalResultBaseMatcher
    : public ::testing::MatcherInterface<const HostResolverInternalResult&> {
 public:
  HostResolverInternalResultBaseMatcher(
      std::string expected_domain_name,
      DnsQueryType expected_query_type,
      HostResolverInternalResult::Source expected_source,
      Matcher<std::optional<base::TimeTicks>> expiration_matcher,
      Matcher<std::optional<base::Time>> timed_expiration_matcher)
      : expected_domain_name_(std::move(expected_domain_name)),
        expected_query_type_(expected_query_type),
        expected_source_(expected_source),
        expiration_matcher_(std::move(expiration_matcher)),
        timed_expiration_matcher_(std::move(timed_expiration_matcher)) {}
  ~HostResolverInternalResultBaseMatcher() override = default;

  bool MatchAndExplain(const HostResolverInternalResult& result,
                       MatchResultListener* result_listener) const override {
    if (result.type() == GetSubtype()) {
      *result_listener << "which is type ";
      NameSubtype(*result_listener);
    } else {
      *result_listener << "which is not type ";
      NameSubtype(*result_listener);
      return false;
    }

    StringMatchResultListener base_listener;
    bool base_matches = MatchAndExplainBaseProperties(result, base_listener);
    StringMatchResultListener subtype_listener;
    bool subtype_matches =
        MatchAndExplainSubtypeProperties(result, subtype_listener);

    // If only one part mismatches, just explain that.
    if (!base_matches || subtype_matches) {
      *result_listener << ", and " << base_listener.str();
    }
    if (!subtype_matches || base_matches) {
      *result_listener << ", and " << subtype_listener.str();
    }

    return base_matches && subtype_matches;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "matches ";
    Describe(*os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not match ";
    Describe(*os);
  }

 protected:
  virtual HostResolverInternalResult::Type GetSubtype() const = 0;
  virtual void NameSubtype(MatchResultListener& result_listener) const = 0;
  virtual bool MatchAndExplainSubtypeProperties(
      const HostResolverInternalResult& result,
      MatchResultListener& result_listener) const = 0;
  virtual void DescribeSubtype(std::ostream& os) const = 0;

 private:
  bool MatchAndExplainBaseProperties(
      const HostResolverInternalResult& result,
      MatchResultListener& result_listener) const {
    return ExplainMatchResult(
        AllOf(Property("domain_name", &HostResolverInternalResult::domain_name,
                       Eq(expected_domain_name_)),
              Property("query_type", &HostResolverInternalResult::query_type,
                       Eq(expected_query_type_)),
              Property("source", &HostResolverInternalResult::source,
                       Eq(expected_source_)),
              Property("expiration", &HostResolverInternalResult::expiration,
                       expiration_matcher_),
              Property("timed_expiration",
                       &HostResolverInternalResult::timed_expiration,
                       timed_expiration_matcher_)),
        result, &result_listener);
  }

  void Describe(std::ostream& os) const {
    os << "\n    HostResolverInternalResult {";
    DescribeBase(os);
    DescribeSubtype(os);
    os << "\n    }\n";
  }

  void DescribeBase(std::ostream& os) const {
    StringMatchResultListener subtype_name_listener;
    NameSubtype(subtype_name_listener);

    os << "\n      domain_name: \"" << expected_domain_name_
       << "\"\n      query_type: " << kDnsQueryTypes.at(expected_query_type_)
       << "\n      type: " << subtype_name_listener.str()
       << "\n      source: " << static_cast<int>(expected_source_)
       << "\n      expiration: " << PrintToString(expiration_matcher_)
       << "\n      timed_expiration: "
       << PrintToString(timed_expiration_matcher_);
  }

  std::string expected_domain_name_;
  DnsQueryType expected_query_type_;
  HostResolverInternalResult::Source expected_source_;
  Matcher<std::optional<base::TimeTicks>> expiration_matcher_;
  Matcher<std::optional<base::Time>> timed_expiration_matcher_;
};

class HostResolverInternalDataResultMatcher
    : public HostResolverInternalResultBaseMatcher {
 public:
  HostResolverInternalDataResultMatcher(
      std::string expected_domain_name,
      DnsQueryType expected_query_type,
      HostResolverInternalResult::Source expected_source,
      Matcher<std::optional<base::TimeTicks>> expiration_matcher,
      Matcher<std::optional<base::Time>> timed_expiration_matcher,
      Matcher<std::vector<IPEndPoint>> endpoints_matcher,
      Matcher<std::vector<std::string>> strings_matcher,
      Matcher<std::vector<HostPortPair>> hosts_matcher)
      : HostResolverInternalResultBaseMatcher(
            std::move(expected_domain_name),
            expected_query_type,
            expected_source,
            std::move(expiration_matcher),
            std::move(timed_expiration_matcher)),
        endpoints_matcher_(std::move(endpoints_matcher)),
        strings_matcher_(std::move(strings_matcher)),
        hosts_matcher_(std::move(hosts_matcher)) {}

  ~HostResolverInternalDataResultMatcher() override = default;

  HostResolverInternalDataResultMatcher(
      const HostResolverInternalDataResultMatcher&) = default;
  HostResolverInternalDataResultMatcher& operator=(
      const HostResolverInternalDataResultMatcher&) = default;
  HostResolverInternalDataResultMatcher(
      HostResolverInternalDataResultMatcher&&) = default;
  HostResolverInternalDataResultMatcher& operator=(
      HostResolverInternalDataResultMatcher&&) = default;

 protected:
  HostResolverInternalResult::Type GetSubtype() const override {
    return HostResolverInternalResult::Type::kData;
  }

  void NameSubtype(MatchResultListener& result_listener) const override {
    result_listener << "kData";
  }

  bool MatchAndExplainSubtypeProperties(
      const HostResolverInternalResult& result,
      MatchResultListener& result_listener) const override {
    return ExplainMatchResult(
        AllOf(Property("endpoints", &HostResolverInternalDataResult::endpoints,
                       endpoints_matcher_),
              Property("strings", &HostResolverInternalDataResult::strings,
                       strings_matcher_),
              Property("hosts", &HostResolverInternalDataResult::hosts,
                       hosts_matcher_)),
        result.AsData(), &result_listener);
  }

  void DescribeSubtype(std::ostream& os) const override {
    os << "\n      endpoints: " << PrintToString(endpoints_matcher_)
       << "\n      strings: " << PrintToString(strings_matcher_)
       << "\n      hosts: " << PrintToString(hosts_matcher_);
  }

 private:
  Matcher<std::vector<IPEndPoint>> endpoints_matcher_;
  Matcher<std::vector<std::string>> strings_matcher_;
  Matcher<std::vector<HostPortPair>> hosts_matcher_;
};

class HostResolverInternalMetadataResultMatcher
    : public HostResolverInternalResultBaseMatcher {
 public:
  HostResolverInternalMetadataResultMatcher(
      std::string expected_domain_name,
      DnsQueryType expected_query_type,
      HostResolverInternalResult::Source expected_source,
      Matcher<std::optional<base::TimeTicks>> expiration_matcher,
      Matcher<std::optional<base::Time>> timed_expiration_matcher,
      Matcher<std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>>
          metadatas_matcher)
      : HostResolverInternalResultBaseMatcher(
            std::move(expected_domain_name),
            expected_query_type,
            expected_source,
            std::move(expiration_matcher),
            std::move(timed_expiration_matcher)),
        metadatas_matcher_(std::move(metadatas_matcher)) {}

  ~HostResolverInternalMetadataResultMatcher() override = default;

  HostResolverInternalMetadataResultMatcher(
      const HostResolverInternalMetadataResultMatcher&) = default;
  HostResolverInternalMetadataResultMatcher& operator=(
      const HostResolverInternalMetadataResultMatcher&) = default;
  HostResolverInternalMetadataResultMatcher(
      HostResolverInternalMetadataResultMatcher&&) = default;
  HostResolverInternalMetadataResultMatcher& operator=(
      HostResolverInternalMetadataResultMatcher&&) = default;

 protected:
  HostResolverInternalResult::Type GetSubtype() const override {
    return HostResolverInternalResult::Type::kMetadata;
  }

  void NameSubtype(MatchResultListener& result_listener) const override {
    result_listener << "kMetadata";
  }

  bool MatchAndExplainSubtypeProperties(
      const HostResolverInternalResult& result,
      MatchResultListener& result_listener) const override {
    return ExplainMatchResult(
        Property("metadatas", &HostResolverInternalMetadataResult::metadatas,
                 metadatas_matcher_),
        result.AsMetadata(), &result_listener);
  }

  void DescribeSubtype(std::ostream& os) const override {
    os << "\n      metadatas: " << PrintToString(metadatas_matcher_);
  }

 private:
  Matcher<std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>>
      metadatas_matcher_;
};

class HostResolverInternalErrorResultMatcher
    : public HostResolverInternalResultBaseMatcher {
 public:
  HostResolverInternalErrorResultMatcher(
      std::string expected_domain_name,
      DnsQueryType expected_query_type,
      HostResolverInternalResult::Source expected_source,
      Matcher<std::optional<base::TimeTicks>> expiration_matcher,
      Matcher<std::optional<base::Time>> timed_expiration_matcher,
      int expected_error)
      : HostResolverInternalResultBaseMatcher(
            std::move(expected_domain_name),
            expected_query_type,
            expected_source,
            std::move(expiration_matcher),
            std::move(timed_expiration_matcher)),
        expected_error_(expected_error) {}

  ~HostResolverInternalErrorResultMatcher() override = default;

  HostResolverInternalErrorResultMatcher(
      const HostResolverInternalErrorResultMatcher&) = default;
  HostResolverInternalErrorResultMatcher& operator=(
      const HostResolverInternalErrorResultMatcher&) = default;
  HostResolverInternalErrorResultMatcher(
      HostResolverInternalErrorResultMatcher&&) = default;
  HostResolverInternalErrorResultMatcher& operator=(
      HostResolverInternalErrorResultMatcher&&) = default;

 protected:
  HostResolverInternalResult::Type GetSubtype() const override {
    return HostResolverInternalResult::Type::kError;
  }

  void NameSubtype(MatchResultListener& result_listener) const override {
    result_listener << "kError";
  }

  bool MatchAndExplainSubtypeProperties(
      const HostResolverInternalResult& result,
      MatchResultListener& result_listener) const override {
    return ExplainMatchResult(
        Property("error", &HostResolverInternalErrorResult::error,
                 Eq(expected_error_)),
        result.AsError(), &result_listener);
  }

  void DescribeSubtype(std::ostream& os) const override {
    os << "\n      error: " << expected_error_;
  }

 private:
  int expected_error_;
};

class HostResolverInternalAliasResultMatcher
    : public HostResolverInternalResultBaseMatcher {
 public:
  HostResolverInternalAliasResultMatcher(
      std::string expected_domain_name,
      DnsQueryType expected_query_type,
      HostResolverInternalResult::Source expected_source,
      Matcher<std::optional<base::TimeTicks>> expiration_matcher,
      Matcher<std::optional<base::Time>> timed_expiration_matcher,
      std::string expected_alias_target)
      : HostResolverInternalResultBaseMatcher(
            std::move(expected_domain_name),
            expected_query_type,
            expected_source,
            std::move(expiration_matcher),
            std::move(timed_expiration_matcher)),
        expected_alias_target_(std::move(expected_alias_target)) {}

  ~HostResolverInternalAliasResultMatcher() override = default;

  HostResolverInternalAliasResultMatcher(
      const HostResolverInternalAliasResultMatcher&) = default;
  HostResolverInternalAliasResultMatcher& operator=(
      const HostResolverInternalAliasResultMatcher&) = default;
  HostResolverInternalAliasResultMatcher(
      HostResolverInternalAliasResultMatcher&&) = default;
  HostResolverInternalAliasResultMatcher& operator=(
      HostResolverInternalAliasResultMatcher&&) = default;

 protected:
  HostResolverInternalResult::Type GetSubtype() const override {
    return HostResolverInternalResult::Type::kAlias;
  }

  void NameSubtype(MatchResultListener& result_listener) const override {
    result_listener << "kAlias";
  }

  bool MatchAndExplainSubtypeProperties(
      const HostResolverInternalResult& result,
      MatchResultListener& result_listener) const override {
    return ExplainMatchResult(
        Property("alias_target", &HostResolverInternalAliasResult::alias_target,
                 Eq(expected_alias_target_)),
        result.AsAlias(), &result_listener);
  }

  void DescribeSubtype(std::ostream& os) const override {
    os << "\n      target: \"" << expected_alias_target_ << "\"";
  }

 private:
  std::string expected_alias_target_;
};

}  // namespace

Matcher<const HostResolverInternalResult&> ExpectHostResolverInternalDataResult(
    std::string expected_domain_name,
    DnsQueryType expected_query_type,
    HostResolverInternalResult::Source expected_source,
    Matcher<std::optional<base::TimeTicks>> expiration_matcher,
    Matcher<std::optional<base::Time>> timed_expiration_matcher,
    Matcher<std::vector<IPEndPoint>> endpoints_matcher,
    Matcher<std::vector<std::string>> strings_matcher,
    Matcher<std::vector<HostPortPair>> hosts_matcher) {
  return MakeMatcher(new HostResolverInternalDataResultMatcher(
      std::move(expected_domain_name), expected_query_type, expected_source,
      std::move(expiration_matcher), std::move(timed_expiration_matcher),
      std::move(endpoints_matcher), std::move(strings_matcher),
      std::move(hosts_matcher)));
}

testing::Matcher<const HostResolverInternalResult&>
ExpectHostResolverInternalMetadataResult(
    std::string expected_domain_name,
    DnsQueryType expected_query_type,
    HostResolverInternalResult::Source expected_source,
    testing::Matcher<std::optional<base::TimeTicks>> expiration_matcher,
    testing::Matcher<std::optional<base::Time>> timed_expiration_matcher,
    testing::Matcher<
        std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>>
        metadatas_matcher) {
  return MakeMatcher(new HostResolverInternalMetadataResultMatcher(
      std::move(expected_domain_name), expected_query_type, expected_source,
      std::move(expiration_matcher), std::move(timed_expiration_matcher),
      std::move(metadatas_matcher)));
}

Matcher<const HostResolverInternalResult&>
ExpectHostResolverInternalErrorResult(
    std::string expected_domain_name,
    DnsQueryType expected_query_type,
    HostResolverInternalResult::Source expected_source,
    Matcher<std::optional<base::TimeTicks>> expiration_matcher,
    Matcher<std::optional<base::Time>> timed_expiration_matcher,
    int expected_error) {
  return MakeMatcher(new HostResolverInternalErrorResultMatcher(
      std::move(expected_domain_name), expected_query_type, expected_source,
      std::move(expiration_matcher), std::move(timed_expiration_matcher),
      expected_error));
}

Matcher<const HostResolverInternalResult&>
ExpectHostResolverInternalAliasResult(
    std::string expected_domain_name,
    DnsQueryType expected_query_type,
    HostResolverInternalResult::Source expected_source,
    Matcher<std::optional<base::TimeTicks>> expiration_matcher,
    Matcher<std::optional<base::Time>> timed_expiration_matcher,
    std::string expected_alias_target) {
  return MakeMatcher(new HostResolverInternalAliasResultMatcher(
      std::move(expected_domain_name), expected_query_type, expected_source,
      std::move(expiration_matcher), std::move(timed_expiration_matcher),
      std::move(expected_alias_target)));
}

std::ostream& operator<<(std::ostream& os,
                         const HostResolverInternalResult& result) {
  std::string json_string;
  CHECK(base::JSONWriter::Write(result.ToValue(), &json_string));
  return os << json_string;
}

}  // namespace net
