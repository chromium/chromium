// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGTextFragmentBuilder_h
#define NGTextFragmentBuilder_h

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_end_effect.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class LayoutObject;
class ShapeResult;
struct NGInlineItemResult;

class CORE_EXPORT NGTextFragmentBuilder final : public NGFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGTextFragmentBuilder(NGInlineNode node, WritingMode writing_mode)
      : NGFragmentBuilder(writing_mode, TextDirection::kLtr),
        inline_node_(node) {}

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
               scoped_refptr<const ShapeResult>);

  // Creates the fragment. Can only be called once.
  scoped_refptr<const NGPhysicalTextFragment> ToTextFragment();

 private:
  NGInlineNode inline_node_;
  String text_;
  unsigned item_index_;
  unsigned start_offset_;
  unsigned end_offset_;
  scoped_refptr<const ShapeResult> shape_result_;

  NGPhysicalTextFragment::NGTextType text_type_ =
      NGPhysicalTextFragment::kNormalText;

  NGTextEndEffect end_effect_ = NGTextEndEffect::kNone;

  friend class NGPhysicalTextFragment;
};

}  // namespace blink

#endif  // NGTextFragmentBuilder
