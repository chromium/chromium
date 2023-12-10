// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEME_HOST_PORT_MATCHER_RULE_H_
#define NET_BASE_SCHEME_HOST_PORT_MATCHER_RULE_H_

#include <memory>
#include <string>
#include <string_view>

#include "net/base/cronet_buildflags.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/scheme_host_port_matcher_result.h"
#include "url/gurl.h"

namespace net {

// Interface for an individual SchemeHostPortMatcher rule.
class NET_EXPORT SchemeHostPortMatcherRule {
 public:
  SchemeHostPortMatcherRule() = default;
  SchemeHostPortMatcherRule(const SchemeHostPortMatcherRule&) = delete;
  SchemeHostPortMatcherRule& operator=(const SchemeHostPortMatcherRule&) =
      delete;

  virtual ~SchemeHostPortMatcherRule() = default;

  // Creates a SchemeHostPortMatcherRule by best-effort parsing the string. If
  // it can't parse, returns a nullptr. It only parses all the rule types in
  // this header file. Types with other serializations will need to be handled
  // by the caller.
  static std::unique_ptr<SchemeHostPortMatcherRule> FromUntrimmedRawString(
      std::string_view raw_untrimmed);

  // Evaluates the rule against |url|.
  virtual SchemeHostPortMatcherResult Evaluate(const GURL& url) const = 0;
  // Returns a string representation of this rule. The returned string will not
  // match any distinguishable rule of any type.
  virtual std::string ToString() const = 0;
  // Returns true if |this| is an instance of
  // SchemeHostPortMatcherHostnamePatternRule.
  virtual bool IsHostnamePatternRule() const;

#if !BUILDFLAG(CRONET_BUILD)
  // Cronet disables tracing and doesn't provide an implementation of
  // base::trace_event::EstimateMemoryUsage. Having this conditional is
  // preferred over a fake implementation to avoid reporting fake metrics.

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  virtual size_t EstimateMemoryUsage() const;
#endif  // !BUILDFLAG(CRONET_BUILD)
};

// Rule that matches URLs with wildcard hostname patterns, and
// scheme/port restrictions.
//
// For example:
//   *.google.com
//   https://*.google.com
//   google.com:443
class NET_EXPORT SchemeHostPortMatcherHostnamePatternRule
    : public SchemeHostPortMatcherRule {
 public:
  SchemeHostPortMatcherHostnamePatternRule(const std::string& optional_scheme,
                                           const std::string& hostname_pattern,
                                           int optional_port);
  SchemeHostPortMatcherHostnamePatternRule(
      const SchemeHostPortMatcherHostnamePatternRule&) = delete;
  SchemeHostPortMatcherHostnamePatternRule& operator=(
      const SchemeHostPortMatcherHostnamePatternRule&) = delete;

  // SchemeHostPortMatcherRule implementation:
  SchemeHostPortMatcherResult Evaluate(const GURL& url) const override;
  std::string ToString() const override;
  bool IsHostnamePatternRule() const override;

  // Generates a new SchemeHostPortMatcherHostnamePatternRule based on the
  // current rule. The new rule will do suffix matching if the current rule
  // doesn't. For example, "google.com" would become "*google.com" and match
  // "foogoogle.com".
  std::unique_ptr<SchemeHostPortMatcherHostnamePatternRule>
  GenerateSuffixMatchingRule() const;

#if !BUILDFLAG(CRONET_BUILD)
  // Cronet disables tracing and doesn't provide an implementation of
  // base::trace_event::EstimateMemoryUsage. Having this conditional is
  // preferred over a fake implementation to avoid reporting fake metrics.

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;
#endif  // !BUILDFLAG(CRONET_BUILD)

 private:
  const std::string optional_scheme_;
  const std::string hostname_pattern_;
  const int optional_port_;
};

// Rule that matches URLs with IP address as hostname, and scheme/port
// restrictions. * only works in the host portion. i18n domain names must be
// input in punycode format.
//
// For example:
//   127.0.0.1,
//   http://127.0.0.1
//   [::1]
//   [0:0::1]
//   http://[::1]:99
class NET_EXPORT SchemeHostPortMatcherIPHostRule
    : public SchemeHostPortMatcherRule {
 public:
  SchemeHostPortMatcherIPHostRule(const std::string& optional_scheme,
                                  const IPEndPoint& ip_end_point);
  SchemeHostPortMatcherIPHostRule(const SchemeHostPortMatcherIPHostRule&) =
      delete;
  SchemeHostPortMatcherIPHostRule& operator=(
      const SchemeHostPortMatcherIPHostRule&) = delete;

  // SchemeHostPortMatcherRule implementation:
  SchemeHostPortMatcherResult Evaluate(const GURL& url) const override;
  std::string ToString() const override;

#if !BUILDFLAG(CRONET_BUILD)
  // Cronet disables tracing and doesn't provide an implementation of
  // base::trace_event::EstimateMemoryUsage. Having this conditional is
  // preferred over a fake implementation to avoid reporting fake metrics.

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;
#endif  // !BUILDFLAG(CRONET_BUILD)

 private:
  const std::string optional_scheme_;
  const std::string ip_host_;
  const int optional_port_;
};

// Rule for matching a URL that is an IP address, if that IP address falls
// within a certain numeric range.
//
// For example:
//   127.0.0.1/8.
//   FE80::/10
//   but not http://127.0.0.1:7/8 or http://[FE80::]/10 (IPv6 with brackets).
class NET_EXPORT SchemeHostPortMatcherIPBlockRule
    : public SchemeHostPortMatcherRule {
 public:
  // |ip_prefix| + |prefix_length| define the IP block to match.
  SchemeHostPortMatcherIPBlockRule(const std::string& description,
                                   const std::string& optional_scheme,
                                   const IPAddress& ip_prefix,
                                   size_t prefix_length_in_bits);
  SchemeHostPortMatcherIPBlockRule(const SchemeHostPortMatcherIPBlockRule&) =
      delete;
  SchemeHostPortMatcherIPBlockRule& operator=(
      const SchemeHostPortMatcherIPBlockRule&) = delete;

  // SchemeHostPortMatcherRule implementation:
  SchemeHostPortMatcherResult Evaluate(const GURL& url) const override;
  std::string ToString() const override;

#if !BUILDFLAG(CRONET_BUILD)
  // Cronet disables tracing and doesn't provide an implementation of
  // base::trace_event::EstimateMemoryUsage. Having this conditional is
  // preferred over a fake implementation to avoid reporting fake metrics.

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;
#endif  // !BUILDFLAG(CRONET_BUILD)

 private:
  const std::string description_;
  const std::string optional_scheme_;
  const IPAddress ip_prefix_;
  const size_t prefix_length_in_bits_;
};

}  // namespace net

#endif  // NET_BASE_SCHEME_HOST_PORT_MATCHER_RULE_H_
