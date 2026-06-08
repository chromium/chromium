// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_ids.h"

#include <stddef.h>

#include <type_traits>
#include <variant>

namespace chrome_pdf {

static_assert(
    std::is_same_v<InkStrokeId::underlying_type, size_t> &&
        std::is_same_v<InkTextId::underlying_type, size_t>,
    "User-added Ink IDs must be size_t for unified chronological order.");

size_t GetIdTypeValue(const IdType& id) {
  return std::visit([](const auto& v) { return v.value(); }, id);
}

IdType TextIdToIdType(const TextId& id) {
  return std::visit([](const auto& v) -> IdType { return v; }, id);
}

}  // namespace chrome_pdf
