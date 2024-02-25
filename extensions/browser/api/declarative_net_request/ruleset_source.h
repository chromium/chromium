// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"

namespace extensions::declarative_net_request {
class ParseInfo;
class RulesetMatcher;

// Encapsulates information for a single extension ruleset.
class RulesetSource {
 public:
  // Bitflags to configure rule parsing behavior.
  enum ParseFlags {
    // Ignore all invalid or large regex rules.
    kNone = 0,

    // When an error is raised for a rule, further rule parsing is stopped. When
    // a warning is raised for a rule, the problematic rule is skipped, but the
    // parsing of the remaining rules continues. It is not possible to raise
    // both an error and a warning for a rule.

    kRaiseErrorOnInvalidRules = 1 << 0,
    kRaiseWarningOnInvalidRules = 1 << 1,

    kRaiseErrorOnLargeRegexRules = 1 << 2,
    kRaiseWarningOnLargeRegexRules = 1 << 3
  };

  RulesetSource(RulesetID id,
                size_t rule_count_limit,
                ExtensionId extension_id,
                bool enabled);
  virtual ~RulesetSource();
  RulesetSource(RulesetSource&&);
  RulesetSource& operator=(RulesetSource&&);
  RulesetSource(const RulesetSource&) = delete;
  RulesetSource& operator=(const RulesetSource&) = delete;

  // Each ruleset source within an extension has a distinct ID.
  RulesetID id() const { return id_; }

  // The maximum number of rules that will be indexed from this source.
  size_t rule_count_limit() const { return rule_count_limit_; }

  // The ID of the extension from which the ruleset originates from.
  const ExtensionId& extension_id() const { return extension_id_; }

  // Whether the ruleset is enabled by default (as specified in the extension
  // manifest for a static ruleset). Always true for a dynamic ruleset.
  bool enabled_by_default() const { return enabled_by_default_; }

  // Indexes the given |rules| in indexed/flatbuffer format.
  ParseInfo IndexRules(std::vector<api::declarative_net_request::Rule> rules,
                       uint8_t parse_flags) const;

  // Creates a verified RulesetMatcher corresponding to the buffer in |data|.
  // Returns kSuccess on success along with the ruleset |matcher|.
  LoadRulesetResult CreateVerifiedMatcher(
      std::string data,
      std::unique_ptr<RulesetMatcher>* matcher) const;

 private:
  RulesetID id_;
  size_t rule_count_limit_;
  ExtensionId extension_id_;
  bool enabled_by_default_;
};

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
