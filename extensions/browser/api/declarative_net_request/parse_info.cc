// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/parse_info.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions::declarative_net_request {

ParseInfo::ParseInfo(size_t rules_count,
                     size_t regex_rules_count,
                     std::vector<RuleWarning> rule_ignored_warnings,
                     flatbuffers::DetachedBuffer buffer,
                     int ruleset_checksum)
    : has_error_(false),
      rules_count_(rules_count),
      regex_rules_count_(regex_rules_count),
      buffer_(std::move(buffer)),
      ruleset_checksum_(ruleset_checksum),
      rule_ignored_warnings_(std::move(rule_ignored_warnings)) {}

ParseInfo::ParseInfo(ParseResult error_reason, int rule_id)
    : has_error_(true),
      error_(GetParseError(error_reason, rule_id)),
      error_reason_(error_reason) {}

ParseInfo::ParseInfo(ParseInfo&&) = default;
ParseInfo& ParseInfo::operator=(ParseInfo&&) = default;
ParseInfo::~ParseInfo() = default;

}  // namespace extensions::declarative_net_request
