// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_BYPASS_RULES_H_
#define NET_PROXY_RESOLUTION_PROXY_BYPASS_RULES_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "net/base/net_export.h"
#include "net/base/scheme_host_port_matcher.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "url/gurl.h"

namespace net {

// ProxyBypassRules describes the set of URLs that should bypass the use of a
// proxy.
//
// The rules are expressed as an ordered list of rules, which can be thought of
// as being evaluated left-to-right. Order only matters when mixing "negative
// rules" with "positive rules". For more details see the comments in
// ProxyBypassRules::Matches().
//
// This rule list is serializable to a string (either comma or semi-colon
// separated), which has similar semantics across platforms.
//
// When evalutating ProxyBypassRules there are some implicitly applied rules
// when the URL does not match any of the explicit rules. See
// MatchesImplicitRules() for details.
class NET_EXPORT ProxyBypassRules {
 public:
  // Note: This class supports copy constructor and assignment.
  ProxyBypassRules();
  ProxyBypassRules(const ProxyBypassRules& rhs);
  ProxyBypassRules(ProxyBypassRules&& rhs);
  ~ProxyBypassRules();
  ProxyBypassRules& operator=(const ProxyBypassRules& rhs);
  ProxyBypassRules& operator=(ProxyBypassRules&& rhs);

  // Returns the current list of rules. The rules list contains pointers
  // which are owned by this class, callers should NOT keep references
  // or delete them.
  const SchemeHostPortMatcher::RuleList& rules() const {
    return matcher_.rules();
  }

  // Replace rule on |index| in the internal RuleList.
  void ReplaceRule(size_t index,
                   std::unique_ptr<SchemeHostPortMatcherRule> rule);

  // Returns true if the bypass rules indicate that |url| should bypass the
  // proxy. Matching is done using both the explicit rules, as well as a
  // set of global implicit rules.
  //
  // If |reverse| is set to true then the bypass
  // rule list is inverted (this is almost equivalent to negating the result of
  // Matches(), except for implicit matches).
  bool Matches(const GURL& url, bool reverse = false) const;

  // Returns true if |*this| has the same serialized list of rules as |other|.
  bool operator==(const ProxyBypassRules& other) const;

  // Initializes the list of rules by parsing the string |raw|. |raw| is a
  // comma separated or semi-colon separated list of rules. See
  // AddRuleFromString() to see the specific rule grammar.
  void ParseFromString(const std::string& raw);

  // Adds a rule to the front of thelist that bypasses hostnames without a dot
  // in them (and is not an IP literal), which can be indicative of intranet
  // websites.
  //
  // On Windows this corresponds to the "Bypass proxy server for local
  // addresses" settings checkbox, and on macOS the "Exclude simple hostnames"
  // checkbox.
  void PrependRuleToBypassSimpleHostnames();

  // Adds a rule given by the string |raw|. The format of |raw| can be any of
  // the following:
  //
  // Returns true if the rule was successfully added.
  //
  // For the supported format of bypass rules see //net/docs/proxy.md.
  bool AddRuleFromString(std::string_view raw);

  // Appends rules that "cancels out" the implicit bypass rules. See
  // GetRulesToSubtractImplicit() for details.
  void AddRulesToSubtractImplicit();

  // Returns a list of bypass rules that "cancels out" the implicit bypass
  // rules.
  //
  // The current set of implicit bypass rules are localhost and link-local
  // addresses, and are subtracted using <-loopback> (an idiom from Windows),
  // however this could change.
  //
  // If using this for tests, see https://crbug.com/901896.
  static std::string GetRulesToSubtractImplicit();

  // Converts the rules to a string representation (ParseFormat::kDefault).
  std::string ToString() const;

  // Removes all the rules.
  void Clear();

  // Returns true if |url| matches one of the implicit proxy bypass rules
  // (localhost or link local).
  static bool MatchesImplicitRules(const GURL& url);

  // The delimiter used by |ToString()| for the string representation of the
  // proxy bypass rules.
  constexpr static char kBypassListDelimeter[] = ";";

 private:
  SchemeHostPortMatcher matcher_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_BYPASS_RULES_H_
