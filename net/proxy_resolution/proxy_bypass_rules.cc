// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_bypass_rules.h"

#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/base/url_util.h"

namespace net {

namespace {

// The <-loopback> rule corresponds with "remove the implicitly added bypass
// rules".
//
// The name <-loopback> is not a very precise name (as the implicit rules cover
// more than strictly loopback addresses), however this is the name that is
// used on Windows so re-used here.
//
// For platform-differences between implicit rules see
// ProxyResolverRules::MatchesImplicitRules().
const char kSubtractImplicitBypasses[] = "<-loopback>";

// The <local> rule bypasses any hostname that has no dots (and is not
// an IP literal). The name is misleading as it has nothing to do with
// localhost/loopback addresses, and would have better been called
// something like "simple hostnames". However this is the name used on
// Windows so is matched here.
const char kBypassSimpleHostnames[] = "<local>";

bool IsLinkLocalIP(const GURL& url) {
  // Quick fail if definitely not link-local, to avoid doing unnecessary work in
  // common case.
  if (!(url.host_piece().starts_with("169.254.") ||
        url.host_piece().starts_with("["))) {
    return false;
  }

  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(url.HostNoBracketsPiece()))
    return false;

  return ip_address.IsLinkLocal();
}

// Returns true if the URL's host is an IPv6 literal in the range
// [::ffff:127.0.0.1]/104.
//
// Note that net::IsLocalhost() does not currently return true for such
// addresses. However for proxy resolving such URLs should bypass the use
// of a PAC script, since the destination is local.
bool IsIPv4MappedLoopback(const GURL& url) {
  if (!url.host_piece().starts_with("[::ffff")) {
    return false;
  }

  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(url.HostNoBracketsPiece()))
    return false;

  if (!ip_address.IsIPv4MappedIPv6())
    return false;

  return ip_address.bytes()[12] == 127;
}

class BypassSimpleHostnamesRule : public SchemeHostPortMatcherRule {
 public:
  BypassSimpleHostnamesRule() = default;

  BypassSimpleHostnamesRule(const BypassSimpleHostnamesRule&) = delete;
  BypassSimpleHostnamesRule& operator=(const BypassSimpleHostnamesRule&) =
      delete;

  SchemeHostPortMatcherResult Evaluate(const GURL& url) const override {
    return ((url.host_piece().find('.') == std::string::npos) &&
            !url.HostIsIPAddress())
               ? SchemeHostPortMatcherResult::kInclude
               : SchemeHostPortMatcherResult::kNoMatch;
  }

  std::string ToString() const override { return kBypassSimpleHostnames; }
};

class SubtractImplicitBypassesRule : public SchemeHostPortMatcherRule {
 public:
  SubtractImplicitBypassesRule() = default;

  SubtractImplicitBypassesRule(const SubtractImplicitBypassesRule&) = delete;
  SubtractImplicitBypassesRule& operator=(const SubtractImplicitBypassesRule&) =
      delete;

  SchemeHostPortMatcherResult Evaluate(const GURL& url) const override {
    return ProxyBypassRules::MatchesImplicitRules(url)
               ? SchemeHostPortMatcherResult::kExclude
               : SchemeHostPortMatcherResult::kNoMatch;
  }

  std::string ToString() const override { return kSubtractImplicitBypasses; }
};

std::unique_ptr<SchemeHostPortMatcherRule> ParseRule(
    std::string_view raw_untrimmed) {
  std::string_view raw =
      base::TrimWhitespaceASCII(raw_untrimmed, base::TRIM_ALL);

  // <local> and <-loopback> are special syntax used by WinInet's bypass list
  // -- we allow it on all platforms and interpret it the same way.
  if (base::EqualsCaseInsensitiveASCII(raw, kBypassSimpleHostnames))
    return std::make_unique<BypassSimpleHostnamesRule>();
  if (base::EqualsCaseInsensitiveASCII(raw, kSubtractImplicitBypasses))
    return std::make_unique<SubtractImplicitBypassesRule>();

  return SchemeHostPortMatcherRule::FromUntrimmedRawString(raw_untrimmed);
}

}  // namespace

constexpr char net::ProxyBypassRules::kBypassListDelimeter[];

ProxyBypassRules::ProxyBypassRules() = default;

ProxyBypassRules::ProxyBypassRules(const ProxyBypassRules& rhs) {
  *this = rhs;
}

ProxyBypassRules::ProxyBypassRules(ProxyBypassRules&& rhs) {
  *this = std::move(rhs);
}

ProxyBypassRules::~ProxyBypassRules() = default;

ProxyBypassRules& ProxyBypassRules::operator=(const ProxyBypassRules& rhs) {
  ParseFromString(rhs.ToString());
  return *this;
}

ProxyBypassRules& ProxyBypassRules::operator=(ProxyBypassRules&& rhs) {
  matcher_ = std::move(rhs.matcher_);
  return *this;
}

void ProxyBypassRules::ReplaceRule(
    size_t index,
    std::unique_ptr<SchemeHostPortMatcherRule> rule) {
  matcher_.ReplaceRule(index, std::move(rule));
}

bool ProxyBypassRules::Matches(const GURL& url, bool reverse) const {
  switch (matcher_.Evaluate(url)) {
    case SchemeHostPortMatcherResult::kInclude:
      return !reverse;
    case SchemeHostPortMatcherResult::kExclude:
      return reverse;
    case SchemeHostPortMatcherResult::kNoMatch:
      break;
  }

  // If none of the explicit rules matched, fall back to the implicit rules.
  bool matches_implicit = MatchesImplicitRules(url);
  if (matches_implicit)
    return matches_implicit;

  return reverse;
}

bool ProxyBypassRules::operator==(const ProxyBypassRules& other) const {
  if (rules().size() != other.rules().size())
    return false;

  for (size_t i = 0; i < rules().size(); ++i) {
    if (rules()[i]->ToString() != other.rules()[i]->ToString())
      return false;
  }
  return true;
}

void ProxyBypassRules::ParseFromString(const std::string& raw) {
  Clear();

  base::StringTokenizer entries(
      raw, SchemeHostPortMatcher::kParseRuleListDelimiterList);
  while (entries.GetNext()) {
    AddRuleFromString(entries.token_piece());
  }
}

void ProxyBypassRules::PrependRuleToBypassSimpleHostnames() {
  matcher_.AddAsFirstRule(std::make_unique<BypassSimpleHostnamesRule>());
}

bool ProxyBypassRules::AddRuleFromString(std::string_view raw_untrimmed) {
  auto rule = ParseRule(raw_untrimmed);

  if (rule) {
    matcher_.AddAsLastRule(std::move(rule));
    return true;
  }

  return false;
}

void ProxyBypassRules::AddRulesToSubtractImplicit() {
  matcher_.AddAsLastRule(std::make_unique<SubtractImplicitBypassesRule>());
}

std::string ProxyBypassRules::GetRulesToSubtractImplicit() {
  ProxyBypassRules rules;
  rules.AddRulesToSubtractImplicit();
  return rules.ToString();
}

std::string ProxyBypassRules::ToString() const {
  return matcher_.ToString();
}

void ProxyBypassRules::Clear() {
  matcher_.Clear();
}

bool ProxyBypassRules::MatchesImplicitRules(const GURL& url) {
  // On Windows the implict rules are:
  //
  //     localhost
  //     loopback
  //     127.0.0.1
  //     [::1]
  //     169.254/16
  //     [FE80::]/10
  //
  // And on macOS they are:
  //
  //     localhost
  //     127.0.0.1/8
  //     [::1]
  //     169.254/16
  //
  // Our implicit rules are approximately:
  //
  //     localhost
  //     localhost.
  //     *.localhost
  //     loopback  [Windows only]
  //     loopback. [Windows only]
  //     [::1]
  //     127.0.0.1/8
  //     169.254/16
  //     [FE80::]/10
  return IsLocalhost(url) || IsIPv4MappedLoopback(url) ||
         IsLinkLocalIP(url)
#if BUILDFLAG(IS_WIN)
         // See http://crbug.com/904889
         || (url.host_piece() == "loopback") ||
         (url.host_piece() == "loopback.")
#endif
      ;
}

}  // namespace net
