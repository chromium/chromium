// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_initial_letter.h"

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"

namespace blink {

StyleInitialLetter::StyleInitialLetter() = default;

StyleInitialLetter::StyleInitialLetter(float size)
    : size_(size),
      sink_(base::saturated_cast<int>(std::floor(size))),
      sink_type_(kOmitted) {
  DCHECK_GE(size_, 1);
  DCHECK_GE(sink_, 1);
}

StyleInitialLetter::StyleInitialLetter(float size, int sink)
    : size_(size), sink_(sink), sink_type_(kInteger) {
  DCHECK_GE(size_, 1);
  DCHECK_GE(sink_, 1);
}

StyleInitialLetter::StyleInitialLetter(float size, SinkType sink_type)
    : size_(size),
      sink_(sink_type == kDrop ? base::saturated_cast<int>(std::floor(size))
                               : 1),
      sink_type_(sink_type) {
  DCHECK_GE(size_, 1);
  DCHECK_GE(sink_, 1);
  DCHECK(sink_type_ == kDrop || sink_type_ == kRaise);
}

bool StyleInitialLetter::operator==(const StyleInitialLetter& other) const {
  return size_ == other.size_ && sink_ == other.sink_ &&
         sink_type_ == other.sink_type_;
}

bool StyleInitialLetter::operator!=(const StyleInitialLetter& other) const {
  return !operator==(other);
}

// static
StyleInitialLetter StyleInitialLetter::Drop(float size) {
  return StyleInitialLetter(size, kDrop);
}

// static
StyleInitialLetter StyleInitialLetter::Raise(float size) {
  return StyleInitialLetter(size, kRaise);
}

}  // namespace blink
