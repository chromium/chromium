// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/actions_parser.h"

#include <stdint.h>

#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kMaxInputSize = 100 * 1000;
  if (size > kMaxInputSize) {
    // To avoid spurious timeout and out-of-memory fuzz reports.
    return 0;
  }

  std::optional<base::Value> value = base::JSONReader::Read(
      std::string_view(reinterpret_cast<const char*>(data), size));
  if (!value)
    return 0;
  content::ActionsParser parser(std::move(*value));
  std::ignore = parser.Parse();
  return 0;
}
