// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_TRANSFORMED_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_TRANSFORMED_STRING_H_

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

class LayoutText;

// Represents a text transformed in LayoutText and information on how it was
// collapsed or expanded.  This class is necessary to generate OffsetMappings
// correctly even if text-transform or -webkit-text-security changed the text
// length.
//
// Instances are copyable, and immutable.
class TransformedString {
  STACK_ALLOCATED();

 public:
  explicit TransformedString(StringView view) : view_(view) {}
  static TransformedString CreateFrom(const LayoutText& layout_text);

  const StringView& View() const { return view_; }
  TransformedString Substring(unsigned start, unsigned length) const;
  TransformedString Substring(unsigned start) const {
    return Substring(start, view_.length() - start);
  }

 private:
  const StringView view_;

  // TODO(layout-dev): Add a data member representing information on how view_
  // was collapsed/expanded.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_TRANSFORMED_STRING_H_
