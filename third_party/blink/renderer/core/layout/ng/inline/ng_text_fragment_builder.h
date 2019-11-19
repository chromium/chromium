// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_FRAGMENT_BUILDER_H_

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class ShapeResultView;
struct NGInlineItemResult;

class CORE_EXPORT NGTextFragmentBuilder final : public NGFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGTextFragmentBuilder(WritingMode writing_mode)
      : NGFragmentBuilder(writing_mode, TextDirection::kLtr) {}

  NGTextFragmentBuilder(const NGPhysicalTextFragment& fragment);

  // NOTE: Takes ownership of the shape result within the item result.
  void SetItem(NGPhysicalTextFragment::NGTextType,
               const NGInlineItemsData&,
               NGInlineItemResult*,
               LayoutUnit line_height);

  // Set text for generated text, e.g. hyphen and ellipsis.
  void SetText(LayoutObject*,
               const String& text,
               scoped_refptr<const ComputedStyle>,
               bool is_ellipsis_style,
               scoped_refptr<const ShapeResultView>);

  // Creates the fragment. Can only be called once.
  scoped_refptr<const NGPhysicalTextFragment> ToTextFragment();

 private:
  // Returns true if the text is generated (from, e.g., list marker,
  // pseudo-element, ...) instead of from a DOM text node.
  bool IsGeneratedText() const;

  String text_;
  unsigned start_offset_;
  unsigned end_offset_;
  scoped_refptr<const ShapeResultView> shape_result_;

  NGPhysicalTextFragment::NGTextType text_type_ =
      NGPhysicalTextFragment::kNormalText;

  friend class NGPhysicalTextFragment;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_FRAGMENT_BUILDER_H_
