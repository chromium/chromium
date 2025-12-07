// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

// Caution: This function calls `Clear()` for items in `pieces` even though
// the items are `const`, in order to pass checks in ~StringView().
String StrCat(base::span<const StringView> pieces) {
  size_t size = 0;
  bool is_8bit = true;
  for (const auto& view : pieces) {
    size += view.length();
    if (is_8bit && !view.Is8Bit()) {
      // Like StringBuilder, we check one-length 16bit strings.
      is_8bit = view.length() == 1 && view[0] < 0x0100;
    }
  }

  if (is_8bit) {
    base::span<LChar> buffer;
    auto impl = StringImpl::CreateUninitialized(size, buffer);
    for (const auto& view : pieces) {
      base::span<LChar> sub_buffer = buffer.take_first(view.length());
      if (view.Is8Bit()) {
        sub_buffer.copy_from(view.Span8());
      } else {
        DCHECK_EQ(sub_buffer.size(), 1u);
        DCHECK_LT(view[0], 0x0100);
        sub_buffer[0] = view[0];
      }
#if DCHECK_IS_ON()
      const_cast<StringView&>(view).Clear();
#endif
    }
    return impl;
  }
  base::span<UChar> buffer;
  auto impl = StringImpl::CreateUninitialized(size, buffer);
  for (const auto& view : pieces) {
    base::span<UChar> sub_buffer = buffer.take_first(view.length());
    if (view.Is8Bit()) {
      std::ranges::copy(view.Span8(), sub_buffer.begin());
    } else {
      sub_buffer.copy_from(view.Span16());
    }
#if DCHECK_IS_ON()
    const_cast<StringView&>(view).Clear();
#endif
  }
  return impl;
}

}  // namespace

String StrCat(std::initializer_list<StringView> pieces) {
  return StrCat(base::span(pieces));
}

}  // namespace blink
