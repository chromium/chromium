// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_ids.h"

#include <stddef.h>

#include <variant>

namespace chrome_pdf {

size_t GetIdTypeValue(const IdType& id) {
  return std::visit([](const auto& v) { return v.value(); }, id);
}

}  // namespace chrome_pdf
