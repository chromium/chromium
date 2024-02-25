// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace extensions::declarative_net_request {

// Holds the result of indexing a JSON ruleset.
class ParseInfo {
 public:
  struct RuleWarning {
    int rule_id;
    std::string message;
  };

  // Constructor to be used on success.
  ParseInfo(size_t rules_count,
            size_t regex_rules_count,
            std::vector<RuleWarning> rule_ignored_warnings,
            flatbuffers::DetachedBuffer buffer,
            int ruleset_checksum);

  // Constructor to be used on error.
  ParseInfo(ParseResult error_reason, int rule_id);

  ParseInfo(ParseInfo&&);
  ParseInfo& operator=(ParseInfo&&);
  ~ParseInfo();


  bool has_error() const { return has_error_; }
  ParseResult error_reason() const {
    DCHECK(has_error_);
    return error_reason_;
  }
  const std::string& error() const {
    DCHECK(has_error_);
    return error_;
  }

  const std::vector<RuleWarning>& rule_ignored_warnings() const {
    DCHECK(!has_error_);
    return rule_ignored_warnings_;
  }

  size_t rules_count() const {
    DCHECK(!has_error_);
    return rules_count_;
  }

  size_t regex_rules_count() const {
    DCHECK(!has_error_);
    return regex_rules_count_;
  }

  int ruleset_checksum() const {
    DCHECK(!has_error_);
    return ruleset_checksum_;
  }

  base::span<const uint8_t> GetBuffer() const { return buffer_; }

 private:
  bool has_error_ = false;

  // Only valid iff |has_error_| is true.
  std::string error_;
  ParseResult error_reason_ = ParseResult::NONE;

  // Only valid iff |has_error_| is false.
  size_t rules_count_ = 0;
  size_t regex_rules_count_ = 0;
  flatbuffers::DetachedBuffer buffer_;
  int ruleset_checksum_ = -1;

  // Warnings for rules which could not be parsed and were therefore ignored.
  std::vector<RuleWarning> rule_ignored_warnings_;
};

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_
