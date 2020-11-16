// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_

#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"

namespace extensions {
namespace declarative_net_request {

// Encapsulates information for a single extension ruleset.
class RulesetSource {
 public:
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

 private:
  RulesetID id_;
  size_t rule_count_limit_;
  ExtensionId extension_id_;
  bool enabled_by_default_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
