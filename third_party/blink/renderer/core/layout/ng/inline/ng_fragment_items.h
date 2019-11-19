// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

namespace blink {

class NGFragmentItemsBuilder;

// Represents the inside of an inline formatting context.
//
// During the layout phase, descendants of the inline formatting context is
// transformed to a flat list of |NGFragmentItem| and stored in this class.
class CORE_EXPORT NGFragmentItems {
 public:
  NGFragmentItems(NGFragmentItemsBuilder* builder);

  const Vector<std::unique_ptr<NGFragmentItem>>& Items() const {
    return items_;
  }

  const String& Text(bool first_line) const {
    return UNLIKELY(first_line) ? first_line_text_content_ : text_content_;
  }

 private:
  // TODO(kojii): inline capacity TBD.
  Vector<std::unique_ptr<NGFragmentItem>> items_;
  String text_content_;
  String first_line_text_content_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_FRAGMENT_ITEMS_H_
