// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_BUILDER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_style_variant.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class LayoutObject;

class CORE_EXPORT NGFragmentBuilder {
  STACK_ALLOCATED();

 public:
  const ComputedStyle& Style() const {
    DCHECK(style_);
    return *style_;
  }
  NGFragmentBuilder& SetStyleVariant(NGStyleVariant style_variant) {
    style_variant_ = style_variant;
    return *this;
  }
  NGFragmentBuilder& SetStyle(scoped_refptr<const ComputedStyle> style,
                              NGStyleVariant style_variant) {
    DCHECK(style);
    style_ = std::move(style);
    style_variant_ = style_variant;
    return *this;
  }

  WritingMode GetWritingMode() const { return writing_mode_; }
  TextDirection Direction() const { return direction_; }

  LayoutUnit InlineSize() const { return size_.inline_size; }
  LayoutUnit BlockSize() const { return size_.block_size; }
  const NGLogicalSize& Size() const { return size_; }
  NGFragmentBuilder& SetInlineSize(LayoutUnit inline_size) {
    DCHECK_GE(inline_size, LayoutUnit());
    size_.inline_size = inline_size;
    return *this;
  }
  void SetBlockSize(LayoutUnit block_size) { size_.block_size = block_size; }

  LayoutObject* GetLayoutObject() { return layout_object_; }

 protected:
  NGFragmentBuilder(scoped_refptr<const ComputedStyle> style,
                    WritingMode writing_mode,
                    TextDirection direction)
      : style_(std::move(style)),
        writing_mode_(writing_mode),
        direction_(direction),
        style_variant_(NGStyleVariant::kStandard) {
    DCHECK(style_);
  }
  NGFragmentBuilder(WritingMode writing_mode, TextDirection direction)
      : writing_mode_(writing_mode), direction_(direction) {}

 protected:
  scoped_refptr<const ComputedStyle> style_;
  WritingMode writing_mode_;
  TextDirection direction_;
  NGStyleVariant style_variant_;
  NGLogicalSize size_;
  LayoutObject* layout_object_ = nullptr;
  scoped_refptr<NGBreakToken> break_token_;

  friend class NGPhysicalFragment;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FRAGMENT_BUILDER_H_
