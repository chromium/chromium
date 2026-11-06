// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/image_utils.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace zucchini {

std::string ElementMatch::ToString() const {
  return base::StringPrintf("%" PRIuS "+%" PRIuS "=%" PRIuS "+%" PRIuS "",
                            old_element.offset, old_element.size,
                            new_element.offset, new_element.size);
}

}  // namespace zucchini
