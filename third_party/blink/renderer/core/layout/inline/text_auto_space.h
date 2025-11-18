// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_TEXT_AUTO_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_TEXT_AUTO_SPACE_H_

#include <unicode/umachine.h>

#include <ostream>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_segment.h"
#include "third_party/blink/renderer/core/layout/inline/inline_items_data.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct InlineItemsData;
class InlineNode;

// A wrapper of TextAutoSpace for the inline layout.
class CORE_EXPORT TextAutoSpace {
  STACK_ALLOCATED();

 public:
  // A class for testing to inspect what `TextAutoSpace` did.
  class CORE_EXPORT Callback {
   public:
    virtual void DidApply(base::span<const OffsetWithSpacing>) = 0;
  };

  explicit TextAutoSpace(const InlineItemsData& data);

  // True if this may apply auto-spacing. If this is false, it's safe to skip
  // calling `Apply()`.
  bool MayApply() const { return may_apply_; }

  // Apply auto-spacing as per CSS Text:
  // https://drafts.csswg.org/css-text-4/#propdef-text-autospace
  //
  // The `data` must be the same instance as the one given to the constructor.
  void Apply(const InlineNode& node, InlineItemsData& data);
  void ApplyIfNeeded(const InlineNode& node, InlineItemsData& data) {
    if (MayApply()) [[unlikely]] {
      Apply(node, data);
    }
  }

  void SetCallbackForTesting(Callback* callback) {
    callback_for_testing_ = callback;
  }

 private:
  bool may_apply_ = false;
  Callback* callback_for_testing_ = nullptr;
};

inline TextAutoSpace::TextAutoSpace(const InlineItemsData& data) {
  if (data.text_content.Is8Bit() ||
      std::ranges::none_of(data.items,
                           [](const auto& item) {
                             return item->Type() == InlineItem::kText &&
                                    item->Style()->TextAutospace() !=
                                        ETextAutospace::kNoAutospace;
                           }) ||
      data.text_content.IsAllSpecialCharacters<[](UChar ch) {
        return !Character::MayNeedEastAsianSpacing(ch);
      }>()) {
    return;
  }

  may_apply_ = true;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_TEXT_AUTO_SPACE_H_
