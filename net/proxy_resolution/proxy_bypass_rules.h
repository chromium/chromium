// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_BYPASS_RULES_H_
#define NET_PROXY_RESOLUTION_PROXY_BYPASS_RULES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/net_export.h"
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
  // Interface for an individual proxy bypass rule.
  class NET_EXPORT Rule {
   public:
    // Describes the result of calling Rule::Evaluate() for a particular URL.
    enum class Result {
      // The URL does not match this rule.
      kNoMatch,

      // The URL matches this rule, and should bypass the proxy.
      kBypass,

      // The URL matches this rule, and should NOT bypass the proxy.
      kDontBypass,
    };

    Rule();
    virtual ~Rule();

    // Evaluates the rule against |url|.
    virtual Result Evaluate(const GURL& url) const = 0;

    // Returns a string representation of this rule (using
    // ParseFormat::kDefault).
    virtual std::string ToString() const = 0;

    bool Equals(const Rule& rule) const;

   private:
    DISALLOW_COPY_AND_ASSIGN(Rule);
  };

  // The input format to use when parsing proxy bypass rules. This format
  // only applies when parsing, since once parsed any serialization will be in
  // terms of ParseFormat::kDefault.
  enum class ParseFormat {
    kDefault,

    // Variation of kDefault that interprets hostname patterns as being suffix
    // tests rather than hostname tests. For example, "google.com" would be
    // interpreted as "*google.com" when parsed with this format, and
    // match "foogoogle.com".
    //
    // Only use this format if needed for compatibility when parsing Linux
    // bypass strings.
    kHostnameSuffixMatching,
  };

  typedef std::vector<std::unique_ptr<Rule>> RuleList;

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
  const RuleList& rules() const { return rules_; }

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
  void ParseFromString(const std::string& raw,
                       ParseFormat format = ParseFormat::kDefault);

  // Adds a rule that matches a URL when all of the following are true:
  //  (a) The URL's scheme matches |optional_scheme|, if
  //      |!optional_scheme.empty()|
  //  (b) The URL's hostname matches |hostname_pattern|.
  //  (c) The URL's (effective) port number matches |optional_port| if
  //      |optional_port != -1|
  // Returns true if the rule was successfully added.
  bool AddRuleForHostname(const std::string& optional_scheme,
                          const std::string& hostname_pattern,
                          int optional_port);

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
  bool AddRuleFromString(const std::string& raw,
                         ParseFormat format = ParseFormat::kDefault);

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
  RuleList rules_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_BYPASS_RULES_H_
