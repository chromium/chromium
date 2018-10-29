// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_pattern.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "net/base/ip_address.h"

namespace net {

class IPPattern::ComponentPattern {
 public:
  ComponentPattern();
  void AppendRange(uint32_t min, uint32_t max);
  bool Match(uint32_t value) const;

 private:
  struct Range {
   public:
    Range(uint32_t min, uint32_t max) : minimum(min), maximum(max) {}
    uint32_t minimum;
    uint32_t maximum;
  };
  typedef std::vector<Range> RangeVector;

  RangeVector ranges_;

  DISALLOW_COPY_AND_ASSIGN(ComponentPattern);
};

IPPattern::ComponentPattern::ComponentPattern() = default;

void IPPattern::ComponentPattern::AppendRange(uint32_t min, uint32_t max) {
  ranges_.push_back(Range(min, max));
}

bool IPPattern::ComponentPattern::Match(uint32_t value) const {
  // Simple linear search should be fine, as we usually only have very few
  // distinct ranges to test.
  for (auto range_it = ranges_.begin(); range_it != ranges_.end(); ++range_it) {
    if (range_it->maximum >= value && range_it->minimum <= value)
      return true;
  }
  return false;
}

IPPattern::IPPattern() : is_ipv4_(true) {}

IPPattern::~IPPattern() = default;

bool IPPattern::Match(const IPAddress& address) const {
  if (ip_mask_.empty())
    return false;
  if (address.IsIPv4() != is_ipv4_)
    return false;

  auto pattern_it(component_patterns_.begin());
  int fixed_value_index = 0;
  // IPv6 |address| vectors have 16 pieces, while our  |ip_mask_| has only
  // 8, so it is easier to count separately.
  int address_index = 0;
  for (size_t i = 0; i < ip_mask_.size(); ++i) {
    uint32_t value_to_test = address.bytes()[address_index++];
    if (!is_ipv4_) {
      value_to_test = (value_to_test << 8) + address.bytes()[address_index++];
    }
    if (ip_mask_[i]) {
      if (component_values_[fixed_value_index++] != value_to_test)
        return false;
      continue;
    }
    if (!(*pattern_it)->Match(value_to_test))
      return false;
    ++pattern_it;
  }
  return true;
}

bool IPPattern::ParsePattern(const std::string& ip_pattern) {
  DCHECK(ip_mask_.empty());
  if (ip_pattern.find(':') != std::string::npos) {
    is_ipv4_ = false;
  }

  std::vector<base::StringPiece> components =
      base::SplitStringPiece(ip_pattern, is_ipv4_ ? "." : ":",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (components.size() != (is_ipv4_ ? 4u : 8u)) {
    DVLOG(1) << "Invalid component count: " << ip_pattern;
    return false;
  }
  for (base::StringPiece component : components) {
    if (component.empty()) {
      DVLOG(1) << "Empty component: " << ip_pattern;
      return false;
    }
    if (component == "*") {
      // Let standard code handle this below.
      component = is_ipv4_ ? "[0-255]" : "[0-FFFF]";
    } else if (component[0] != '[') {
      // This value will just have a specific integer to match.
      uint32_t value;
      if (!ValueTextToInt(component, &value))
        return false;
      ip_mask_.push_back(true);
      component_values_.push_back(value);
      continue;
    }
    if (component.back() != ']') {
      DVLOG(1) << "Missing close bracket: " << ip_pattern;
      return false;
    }
    // Now we know the size() is at least 2.
    if (component.size() == 2) {
      DVLOG(1) << "Empty bracket: " << ip_pattern;
      return false;
    }
    // We'll need a pattern to match this bracketed component.
    std::unique_ptr<ComponentPattern> component_pattern(new ComponentPattern);
    // Trim leading and trailing bracket before calling for parsing.
    if (!ParseComponentPattern(component.substr(1, component.size() - 2),
                               component_pattern.get())) {
      return false;
    }
    ip_mask_.push_back(false);
    component_patterns_.push_back(std::move(component_pattern));
  }
  return true;
}

bool IPPattern::ParseComponentPattern(const base::StringPiece& text,
                                      ComponentPattern* pattern) const {
  // We're given a comma separated set of ranges, some of which may be simple
  // constants.
  for (const std::string& range : base::SplitString(
           text, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    base::StringTokenizer range_pair(range, "-");
    uint32_t min = 0;
    range_pair.GetNext();
    if (!ValueTextToInt(range_pair.token_piece(), &min))
      return false;
    uint32_t max = min;  // Sometimes we have no distinct max.
    if (range_pair.GetNext()) {
      if (!ValueTextToInt(range_pair.token_piece(), &max))
        return false;
    }
    if (range_pair.GetNext()) {
      // Too many "-" in this range specifier.
      DVLOG(1) << "Too many hyphens in range: ";
      return false;
    }
    pattern->AppendRange(min, max);
  }
  return true;
}

bool IPPattern::ValueTextToInt(const base::StringPiece& input,
                               uint32_t* output) const {
  bool ok = is_ipv4_ ? base::StringToUint(input, output) :
                       base::HexStringToUInt(input, output);
  if (!ok) {
    DVLOG(1) << "Could not convert value to number: " << input;
    return false;
  }
  if (is_ipv4_ && *output > 255u) {
    DVLOG(1) << "IPv4 component greater than 255";
    return false;
  }
  if (!is_ipv4_ && *output > 0xFFFFu) {
    DVLOG(1) << "IPv6 component greater than 0xFFFF";
    return false;
  }
  return ok;
}

}  // namespace net
