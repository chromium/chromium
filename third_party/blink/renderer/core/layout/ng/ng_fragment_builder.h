// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_BUILDER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_style_variant.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;

class CORE_EXPORT NGFragmentBuilder {
  STACK_ALLOCATED();

 public:
  const ComputedStyle& Style() const {
    DCHECK(style_);
    return *style_;
  }
  void SetStyleVariant(NGStyleVariant style_variant) {
    style_variant_ = style_variant;
  }

  WritingDirectionMode GetWritingDirection() const {
    return writing_direction_;
  }
  WritingMode GetWritingMode() const {
    return writing_direction_.GetWritingMode();
  }
  TextDirection Direction() const { return writing_direction_.Direction(); }

  LayoutUnit InlineSize() const { return size_.inline_size; }
  LayoutUnit BlockSize() const {
    DCHECK(size_.block_size != kIndefiniteSize);
    return size_.block_size;
  }
  const LogicalSize& Size() const {
    DCHECK(size_.block_size != kIndefiniteSize);
    return size_;
  }
  void SetBlockSize(LayoutUnit block_size) { size_.block_size = block_size; }

  bool HasBlockSize() const { return size_.block_size != kIndefiniteSize; }

  void SetIsHiddenForPaint(bool value) { is_hidden_for_paint_ = value; }
  void SetIsOpaque() { is_opaque_ = true; }

  void SetHasCollapsedBorders(bool value) { has_collapsed_borders_ = value; }

  const LayoutObject* GetLayoutObject() const { return layout_object_; }

 protected:
  NGFragmentBuilder(scoped_refptr<const ComputedStyle> style,
                    WritingDirectionMode writing_direction)
      : style_(std::move(style)),
        writing_direction_(writing_direction),
        style_variant_(NGStyleVariant::kStandard) {
    DCHECK(style_);
  }
  explicit NGFragmentBuilder(WritingDirectionMode writing_direction)
      : writing_direction_(writing_direction) {}

  NGFragmentBuilder(const NGPhysicalFragment& fragment)
      : style_(&fragment.Style()),
        writing_direction_(style_->GetWritingDirection()),
        style_variant_(fragment.StyleVariant()),
        size_(fragment.Size().ConvertToLogical(GetWritingMode())),
        layout_object_(fragment.GetMutableLayoutObject()),
        is_hidden_for_paint_(fragment.IsHiddenForPaint()) {}

 protected:
  scoped_refptr<const ComputedStyle> style_;
  WritingDirectionMode writing_direction_;
  NGStyleVariant style_variant_;
  LogicalSize size_;
  LayoutObject* layout_object_ = nullptr;
  const NGBreakToken* break_token_ = nullptr;
  bool is_hidden_for_paint_ = false;
  bool is_opaque_ = false;
  bool has_collapsed_borders_ = false;
  friend class NGPhysicalFragment;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_BUILDER_H_
