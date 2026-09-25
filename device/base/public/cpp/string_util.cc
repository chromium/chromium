// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/public/cpp/string_util.h"

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "base/containers/span_rust.h"
#include "device/base/string_util.rs.h"

namespace device {

bool HasGraphicCharacter(std::string_view s) {
  return has_graphic_character(base::SpanToRustSlice(base::as_byte_span(s)));
}

std::u16string ContainStringForDisplay(std::u16string_view str) {
  if (str.empty()) {
    return std::u16string();
  }

  rust::Slice<const uint16_t> slice(
      reinterpret_cast<const uint16_t*>(str.data()), str.size());
  rust::Vec<uint16_t> contained = contain_string_for_display(slice);
  if (contained.empty()) {
    return std::u16string();
  }

  return std::u16string(contained.begin(), contained.end());
}

}  // namespace device
