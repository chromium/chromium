// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "content/common/input/actions_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kMaxInputSize = 100 * 1000;
  if (size > kMaxInputSize) {
    // To avoid spurious timeout and out-of-memory fuzz reports.
    return 0;
  }

  absl::optional<base::Value> value = base::JSONReader::Read(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  if (!value)
    return 0;
  content::ActionsParser parser(std::move(*value));
  std::ignore = parser.Parse();
  return 0;
}
