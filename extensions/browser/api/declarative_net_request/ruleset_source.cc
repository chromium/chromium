// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_source.h"

namespace extensions {
namespace declarative_net_request {

RulesetSource::RulesetSource(RulesetID id,
                             size_t rule_count_limit,
                             ExtensionId extension_id,
                             bool enabled)
    : id_(id),
      rule_count_limit_(rule_count_limit),
      extension_id_(std::move(extension_id)),
      enabled_by_default_(enabled) {}

RulesetSource::~RulesetSource() = default;
RulesetSource::RulesetSource(RulesetSource&&) = default;
RulesetSource& RulesetSource::operator=(RulesetSource&&) = default;

}  // namespace declarative_net_request
}  // namespace extensions
