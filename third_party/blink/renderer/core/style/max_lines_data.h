// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_MAX_LINES_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_MAX_LINES_DATA_H_

#include <cstdlib>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Represents CSS max-lines: <integer [1,∞]> || auto
//
// TODO(abotella@igalia.com): This doesn't currently match the spec's
// definition; see
// https://github.com/w3c/csswg-drafts/issues/13670#issuecomment-4179997777
class CORE_EXPORT MaxLinesData final {
  DISALLOW_NEW();

 public:
  MaxLinesData(uint16_t lines, bool has_auto)
      : lines_(lines), has_auto_(has_auto) {
    if (lines_ == 0) {
      DCHECK(has_auto_) << "max-lines can't have zero lines without auto";
    }
  }

  bool operator==(const MaxLinesData& other) const {
    return lines_ == other.lines_ && has_auto_ == other.has_auto_;
  }

  unsigned Lines() const { return lines_; }

  bool HasAutoKeyword() const { return has_auto_; }

  bool IsAutoValue() const { return lines_ == 0 && has_auto_; }

 private:
  uint16_t lines_;
  bool has_auto_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_MAX_LINES_DATA_H_
