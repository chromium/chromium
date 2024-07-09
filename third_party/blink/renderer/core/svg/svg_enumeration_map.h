// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ENUMERATION_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ENUMERATION_MAP_H_

#include "base/check.h"
#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace WTF {
class String;
}  // namespace WTF

namespace blink {

// Helper class for SVG enumerations. Maps between name (string) and value.
//
// It is assumed that enumeration values are contiguous, non-zero and
// starting at 1.
//
// For enumerations that have had new values added since SVG 1.1, the
// |max_exposed_value| should be set to the last old value. From this also
// follow that the new values should sort last - after the |max_exposed_value|.
// (This is currently always the case in the spec too.)
class CORE_EXPORT SVGEnumerationMap {
 public:
  struct Entry {
    const uint16_t value;
    const char* const name;
  };

  template <uint16_t entries_length>
  constexpr SVGEnumerationMap(const Entry (&entries)[entries_length])
      : SVGEnumerationMap(entries, entries[entries_length - 1].value) {}
  template <uint16_t entries_length>
  constexpr SVGEnumerationMap(const Entry (&entries)[entries_length],
                              uint16_t max_exposed_value)
      : entries_(entries),
        num_entries_(entries_length),
        max_exposed_value_(max_exposed_value) {}

  const char* NameFromValue(uint16_t value) const {
    DCHECK(value);  // We should never store 0 (*_UNKNOWN) in the map.
    DCHECK_LT(value - 1, num_entries_);
    DCHECK_EQ(entries_[value - 1].value, value);
    return entries_[value - 1].name;
  }
  uint16_t ValueFromName(const WTF::String&) const;

  uint16_t ValueOfLast() const { return entries_[num_entries_ - 1].value; }
  uint16_t MaxExposedValue() const { return max_exposed_value_; }

 private:
  const Entry* begin() const { return entries_; }
  const Entry* end() const { return entries_ + num_entries_; }

  const Entry* const entries_;
  const uint16_t num_entries_;
  const uint16_t max_exposed_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ENUMERATION_MAP_H_
