// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CONTENT_ALIGNMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CONTENT_ALIGNMENT_DATA_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class StyleContentAlignmentData {
  DISALLOW_NEW();

 public:
  // Style data for Content-Distribution properties: align-content,
  // justify-content.
  // <content-distribution> || [ <overflow-position>? && <content-position> ]
  StyleContentAlignmentData(
      ContentPosition position,
      ContentDistributionType distribution,
      OverflowAlignment overflow = OverflowAlignment::kDefault)
      : position_(static_cast<unsigned>(position)),
        distribution_(static_cast<unsigned>(distribution)),
        overflow_(static_cast<unsigned>(overflow)) {}

  void SetPosition(ContentPosition position) {
    position_ = static_cast<unsigned>(position);
  }
  void SetDistribution(ContentDistributionType distribution) {
    distribution_ = static_cast<unsigned>(distribution);
  }
  void SetOverflow(OverflowAlignment overflow) {
    overflow_ = static_cast<unsigned>(overflow);
  }

  // TODO(celestepan): Rename this to `GetComputedPosition()` once we implement
  // returning `flow-start/end` as the serialized value for
  // 'flex-start'/'flex-end'.
  ContentPosition GetPosition() const {
    return static_cast<ContentPosition>(position_);
  }
  // Returns the used position, which is the keyword that we are actually using
  // internally. In the case of 'flex-start'/'flex-end', it aliases to
  // 'flow-start'/'flow-end', which is what we use internally.
  ContentPosition GetUsedPosition() const {
    const ContentPosition position = GetPosition();
    if (position == ContentPosition::kFlexStart) {
      return ContentPosition::kFlowStart;
    }
    if (position == ContentPosition::kFlexEnd) {
      return ContentPosition::kFlowEnd;
    }
    return position;
  }
  ContentDistributionType Distribution() const {
    return static_cast<ContentDistributionType>(distribution_);
  }
  OverflowAlignment Overflow() const {
    return static_cast<OverflowAlignment>(overflow_);
  }

  bool operator==(const StyleContentAlignmentData& o) const {
    return position_ == o.position_ && distribution_ == o.distribution_ &&
           overflow_ == o.overflow_;
  }

 private:
  unsigned position_ : 4;      // ContentPosition
  unsigned distribution_ : 3;  // ContentDistributionType
  unsigned overflow_ : 2;      // OverflowAlignment
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CONTENT_ALIGNMENT_DATA_H_
