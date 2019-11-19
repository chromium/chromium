/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_TRACK_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_TRACK_SIZE_H_

#include "third_party/blink/renderer/core/style/grid_length.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

enum GridTrackSizeType {
  kLengthTrackSizing,
  kMinMaxTrackSizing,
  kFitContentTrackSizing
};

// This class represents a <track-size> from the spec. Althought there are 3
// different types of <track-size> there is always an equivalent minmax()
// representation that could represent any of them. The only special case is
// fit-content(argument) which is similar to minmax(auto, max-content) except
// that the track size is clamped at argument if it is greater than the auto
// minimum. At the GridTrackSize level we don't need to worry about clamping so
// we treat that case exactly as auto.
//
// We're using a separate attribute to store fit-content argument even though we
// could directly use m_maxTrackBreadth. The reason why we don't do it is
// because the maxTrackBreadh() call is a hot spot, so adding a conditional
// statement there (to distinguish between fit-content and any other case) was
// causing a severe performance drop.
class GridTrackSize {
  DISALLOW_NEW();

 public:
  GridTrackSize(const GridLength& length,
                GridTrackSizeType track_size_type = kLengthTrackSizing)
      : type_(track_size_type),
        min_track_breadth_(track_size_type == kFitContentTrackSizing
                               ? Length::Auto()
                               : length),
        max_track_breadth_(track_size_type == kFitContentTrackSizing
                               ? Length::Auto()
                               : length),
        fit_content_track_breadth_(track_size_type == kFitContentTrackSizing
                                       ? length
                                       : GridLength(Length::Fixed())) {
    DCHECK(track_size_type == kLengthTrackSizing ||
           track_size_type == kFitContentTrackSizing);
    DCHECK(track_size_type != kFitContentTrackSizing || length.IsLength());
    CacheMinMaxTrackBreadthTypes();
  }

  GridTrackSize(const GridLength& min_track_breadth,
                const GridLength& max_track_breadth)
      : type_(kMinMaxTrackSizing),
        min_track_breadth_(min_track_breadth),
        max_track_breadth_(max_track_breadth),
        fit_content_track_breadth_(GridLength(Length::Fixed())) {
    CacheMinMaxTrackBreadthTypes();
  }

  const GridLength& FitContentTrackBreadth() const {
    DCHECK(type_ == kFitContentTrackSizing);
    return fit_content_track_breadth_;
  }

  const GridLength& MinTrackBreadth() const { return min_track_breadth_; }
  const GridLength& MaxTrackBreadth() const { return max_track_breadth_; }

  GridTrackSizeType GetType() const { return type_; }

  bool IsContentSized() const {
    return min_track_breadth_.IsContentSized() ||
           max_track_breadth_.IsContentSized();
  }
  bool IsFitContent() const { return type_ == kFitContentTrackSizing; }

  bool operator==(const GridTrackSize& other) const {
    return type_ == other.type_ &&
           min_track_breadth_ == other.min_track_breadth_ &&
           max_track_breadth_ == other.max_track_breadth_ &&
           fit_content_track_breadth_ == other.fit_content_track_breadth_;
  }

  void CacheMinMaxTrackBreadthTypes() {
    min_track_breadth_is_auto_ =
        MinTrackBreadth().IsLength() && MinTrackBreadth().length().IsAuto();
    min_track_breadth_is_min_content_ =
        MinTrackBreadth().IsLength() &&
        MinTrackBreadth().length().IsMinContent();
    min_track_breadth_is_max_content_ =
        MinTrackBreadth().IsLength() &&
        MinTrackBreadth().length().IsMaxContent();
    max_track_breadth_is_max_content_ =
        MaxTrackBreadth().IsLength() &&
        MaxTrackBreadth().length().IsMaxContent();
    max_track_breadth_is_min_content_ =
        MaxTrackBreadth().IsLength() &&
        MaxTrackBreadth().length().IsMinContent();
    max_track_breadth_is_auto_ =
        MaxTrackBreadth().IsLength() && MaxTrackBreadth().length().IsAuto();
    max_track_breadth_is_fixed_ = MaxTrackBreadth().IsLength() &&
                                  MaxTrackBreadth().length().IsSpecified();

    min_track_breadth_is_intrinsic_ = min_track_breadth_is_max_content_ ||
                                      min_track_breadth_is_min_content_ ||
                                      min_track_breadth_is_auto_ ||
                                      IsFitContent();
    max_track_breadth_is_intrinsic_ = max_track_breadth_is_max_content_ ||
                                      max_track_breadth_is_min_content_ ||
                                      max_track_breadth_is_auto_ ||
                                      IsFitContent();
  }

  bool HasIntrinsicMinTrackBreadth() const {
    return min_track_breadth_is_intrinsic_;
  }

  bool HasIntrinsicMaxTrackBreadth() const {
    return max_track_breadth_is_intrinsic_;
  }

  bool HasMinOrMaxContentMinTrackBreadth() const {
    return min_track_breadth_is_max_content_ ||
           min_track_breadth_is_min_content_;
  }
  bool HasAutoMaxTrackBreadth() const { return max_track_breadth_is_auto_; }
  bool HasAutoMinTrackBreadth() const { return min_track_breadth_is_auto_; }
  bool HasMaxContentMinTrackBreadth() const {
    return min_track_breadth_is_max_content_;
  }
  bool HasMinContentMinTrackBreadth() const {
    return min_track_breadth_is_min_content_;
  }
  bool HasMinOrMaxContentMaxTrackBreadth() const {
    return max_track_breadth_is_max_content_ ||
           max_track_breadth_is_min_content_;
  }
  bool HasMaxContentMaxTrackBreadth() const {
    return max_track_breadth_is_max_content_;
  }
  bool HasMaxContentOrAutoMaxTrackBreadth() const {
    return max_track_breadth_is_max_content_ || max_track_breadth_is_auto_;
  }
  bool HasMinContentMaxTrackBreadth() const {
    return max_track_breadth_is_min_content_;
  }
  bool HasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth() const {
    return min_track_breadth_is_max_content_ &&
           max_track_breadth_is_max_content_;
  }
  bool HasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth() const {
    return (min_track_breadth_is_min_content_ || min_track_breadth_is_auto_) &&
           max_track_breadth_is_intrinsic_;
  }
  bool HasFixedMaxTrackBreadth() const { return max_track_breadth_is_fixed_; }

 private:
  GridTrackSizeType type_;
  GridLength min_track_breadth_;
  GridLength max_track_breadth_;
  GridLength fit_content_track_breadth_;

  bool min_track_breadth_is_auto_ : 1;
  bool max_track_breadth_is_auto_ : 1;
  bool min_track_breadth_is_max_content_ : 1;
  bool min_track_breadth_is_min_content_ : 1;
  bool max_track_breadth_is_max_content_ : 1;
  bool max_track_breadth_is_min_content_ : 1;
  bool min_track_breadth_is_intrinsic_ : 1;
  bool max_track_breadth_is_intrinsic_ : 1;
  bool max_track_breadth_is_fixed_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_TRACK_SIZE_H_
