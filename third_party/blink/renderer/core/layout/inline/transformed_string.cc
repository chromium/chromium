// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/transformed_string.h"

#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

// static
TransformedString TransformedString::CreateFrom(const LayoutText& layout_text) {
  // TODO(layout-dev): Get WTF::TextOffsetMap from `layout_text`, and store
  // equivalent information to the TransformedString.
  return TransformedString(layout_text.GetText());
}

TransformedString TransformedString::Substring(unsigned start,
                                               unsigned length) const {
  return TransformedString(StringView(view_, start, length));
}

}  // namespace blink
