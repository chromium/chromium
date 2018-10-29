// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalLineBoxFragment_h
#define NGPhysicalLineBoxFragment_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"

namespace blink {

class NGLineBoxFragmentBuilder;

class CORE_EXPORT NGPhysicalLineBoxFragment final
    : public NGPhysicalContainerFragment {
 public:
  static scoped_refptr<const NGPhysicalLineBoxFragment> Create(
      NGLineBoxFragmentBuilder* builder);

  ~NGPhysicalLineBoxFragment() {
    for (const NGLinkStorage& child : Children())
      child.fragment->Release();
  }

  ChildLinkList Children() const final {
    return ChildLinkList(num_children_, &children_[0]);
  }

  const NGLineHeightMetrics& Metrics() const { return metrics_; }

  // The base direction of this line. Also known as the paragraph direction.
  // This may be different from the direction of the container box when
  // first-line style is used, or when 'unicode-bidi: plaintext' is used.
  TextDirection BaseDirection() const {
    return static_cast<TextDirection>(base_direction_);
  }

  // Compute baseline for the specified baseline type.
  NGLineHeightMetrics BaselineMetrics(FontBaseline) const;

  // Ink overflow of itself including contents, in the local coordinate.
  NGPhysicalOffsetRect InkOverflow() const;

  // Ink overflow of children in local coordinates.
  NGPhysicalOffsetRect ContentsInkOverflow() const;

  // Scrollable overflow. including contents, in the local coordinate.
  // ScrollableOverflow is not precomputed/cached because it cannot be computed
  // when LineBox is generated because it needs container dimensions to
  // resolve relative position of its children.
  NGPhysicalOffsetRect ScrollableOverflow(
      const ComputedStyle* container_style,
      NGPhysicalSize container_physical_size) const;

  // Returns the first/last leaf fragment in the line in logical order. Returns
  // nullptr if the line box is empty.
  const NGPhysicalFragment* FirstLogicalLeaf() const;
  const NGPhysicalFragment* LastLogicalLeaf() const;

  // Whether the content soft-wraps to the next line.
  bool HasSoftWrapToNextLine() const;

 private:
  NGPhysicalLineBoxFragment(NGLineBoxFragmentBuilder* builder);

  NGLineHeightMetrics metrics_;
  NGLinkStorage children_[];
};

DEFINE_TYPE_CASTS(NGPhysicalLineBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentLineBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentLineBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
