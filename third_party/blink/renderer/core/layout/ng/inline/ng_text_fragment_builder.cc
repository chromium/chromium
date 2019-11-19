// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

NGTextFragmentBuilder::NGTextFragmentBuilder(
    const NGPhysicalTextFragment& fragment)
    : NGFragmentBuilder(fragment),
      text_(fragment.text_),
      start_offset_(fragment.StartOffset()),
      end_offset_(fragment.EndOffset()),
      shape_result_(fragment.TextShapeResult()),
      text_type_(fragment.TextType()) {}

void NGTextFragmentBuilder::SetItem(
    NGPhysicalTextFragment::NGTextType text_type,
    const NGInlineItemsData& items_data,
    NGInlineItemResult* item_result,
    LayoutUnit line_height) {
  DCHECK_NE(text_type, NGPhysicalTextFragment::kGeneratedText)
      << "Please use SetText() instead.";
  DCHECK(item_result);
  DCHECK(item_result->item->Style());

  text_type_ = text_type;
  text_ = items_data.text_content;
  start_offset_ = item_result->start_offset;
  end_offset_ = item_result->end_offset;
  SetStyle(item_result->item->Style(), item_result->item->StyleVariant());
  size_ = {item_result->inline_size, line_height};
  shape_result_ = std::move(item_result->shape_result);
  layout_object_ = item_result->item->GetLayoutObject();
}

void NGTextFragmentBuilder::SetText(
    LayoutObject* layout_object,
    const String& text,
    scoped_refptr<const ComputedStyle> style,
    bool is_ellipsis_style,
    scoped_refptr<const ShapeResultView> shape_result) {
  DCHECK(layout_object);
  DCHECK(style);
  DCHECK(shape_result);

  text_type_ = NGPhysicalTextFragment::kGeneratedText;
  text_ = text;
  start_offset_ = shape_result->StartIndex();
  end_offset_ = shape_result->EndIndex();
  SetStyle(style, is_ellipsis_style ? NGStyleVariant::kEllipsis
                                    : NGStyleVariant::kStandard);
  size_ = {shape_result->SnappedWidth(),
           NGLineHeightMetrics(*style).LineHeight()};
  shape_result_ = std::move(shape_result);
  layout_object_ = layout_object;
}

bool NGTextFragmentBuilder::IsGeneratedText() const {
  if (UNLIKELY(style_variant_ == NGStyleVariant::kEllipsis ||
               text_type_ == NGPhysicalTextFragment::kGeneratedText))
    return true;

  DCHECK(layout_object_);
  if (const auto* layout_text_fragment =
          ToLayoutTextFragmentOrNull(layout_object_)) {
    return !layout_text_fragment->AssociatedTextNode();
  }

  const Node* node = layout_object_->GetNode();
  return !node || node->IsPseudoElement();
}

scoped_refptr<const NGPhysicalTextFragment>
NGTextFragmentBuilder::ToTextFragment() {
  scoped_refptr<const NGPhysicalTextFragment> fragment =
      base::AdoptRef(new NGPhysicalTextFragment(this));
  return fragment;
}

}  // namespace blink
