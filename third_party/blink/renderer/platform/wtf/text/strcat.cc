// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace WTF {

String StrCat(base::span<const StringView> pieces) {
  size_t size = 0;
  bool is_8bit = true;
  for (const auto& view : pieces) {
    size += view.length();
    is_8bit = is_8bit && view.Is8Bit();
  }

  if (is_8bit) {
    base::span<LChar> buffer;
    auto impl = StringImpl::CreateUninitialized(size, buffer);
    for (const auto& view : pieces) {
      buffer.take_first(view.length()).copy_from(view.Span8());
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
  }
  return impl;
}

}  // namespace WTF
