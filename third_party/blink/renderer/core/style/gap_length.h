// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_LENGTH_H_

#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class GapLength {
  DISALLOW_NEW();

 public:
  GapLength() : is_normal_(true) {}
  GapLength(const Length& length) : is_normal_(false), length_(length) {
    DCHECK(length.IsSpecified());
  }

  bool IsNormal() const { return is_normal_; }
  const Length& GetLength() const {
    DCHECK(!IsNormal());
    return length_;
  }

  bool operator==(const GapLength& o) const {
    return is_normal_ == o.is_normal_ && length_ == o.length_;
  }

  bool operator!=(const GapLength& o) const {
    return is_normal_ != o.is_normal_ || length_ != o.length_;
  }

 private:
  bool is_normal_;
  Length length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_LENGTH_H_
