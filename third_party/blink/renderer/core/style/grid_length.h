/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_LENGTH_H_

#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This class wraps the <track-breadth> which can be either a <percentage>,
// <length>, min-content, max-content or <flex>. This class avoids spreading the
// knowledge of <flex> throughout the layout directory by adding an new unit to
// Length.h.
class GridLength {
  DISALLOW_NEW();

 public:
  GridLength(const Length& length)
      : length_(length), flex_(0), type_(kLengthType) {}

  explicit GridLength(double flex) : flex_(flex), type_(kFlexType) {}

  bool IsLength() const { return type_ == kLengthType; }
  bool IsFlex() const { return type_ == kFlexType; }

  const Length& length() const {
    DCHECK(IsLength());
    return length_;
  }

  double Flex() const {
    DCHECK(IsFlex());
    return flex_;
  }

  bool HasPercentage() const {
    return type_ == kLengthType && length_.IsPercentOrCalc();
  }

  bool operator==(const GridLength& o) const {
    return length_ == o.length_ && flex_ == o.flex_ && type_ == o.type_;
  }

  bool IsContentSized() const {
    return type_ == kLengthType &&
           (length_.IsAuto() || length_.IsMinContent() ||
            length_.IsMaxContent());
  }

  bool IsAuto() const { return type_ == kLengthType && length_.IsAuto(); }

 private:
  // Ideally we would put the 2 following fields in a union, but Length has a
  // constructor, a destructor and a copy assignment which isn't allowed.
  Length length_;
  double flex_;
  enum GridLengthType { kLengthType, kFlexType };
  GridLengthType type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_LENGTH_H_
