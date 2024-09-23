// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/region_capture_crop_id.h"

#include <inttypes.h>

#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace blink {

base::Token GUIDToToken(const base::Uuid& guid) {
  std::string lowercase = guid.AsLowercaseString();

  // |lowercase| is either empty, or follows the expected pattern.
  // TODO(crbug.com/1260380): Resolve open question of correct treatment
  // of an invalid GUID.
  if (lowercase.empty()) {
    return base::Token();
  }
  DCHECK_EQ(lowercase.length(), 32u + 4u);  // 32 hex-chars; 4 hyphens.

  base::RemoveChars(lowercase, "-", &lowercase);
  DCHECK_EQ(lowercase.length(), 32u);  // 32 hex-chars; 0 hyphens.

  std::string_view string_piece(lowercase);

  uint64_t high = 0;
  bool success = base::HexStringToUInt64(string_piece.substr(0, 16), &high);
  DCHECK(success);

  uint64_t low = 0;
  success = base::HexStringToUInt64(string_piece.substr(16, 16), &low);
  DCHECK(success);

  return base::Token(high, low);
}

base::Uuid TokenToGUID(const base::Token& token) {
  const std::string hex_str = base::StringPrintf("%016" PRIx64 "%016" PRIx64,
                                                 token.high(), token.low());
  const std::string_view hex_string_piece(hex_str);
  const std::string lowercase = base::StrCat(
      {hex_string_piece.substr(0, 8), "-", hex_string_piece.substr(8, 4), "-",
       hex_string_piece.substr(12, 4), "-", hex_string_piece.substr(16, 4), "-",
       hex_string_piece.substr(20, 12)});

  return base::Uuid::ParseLowercase(lowercase);
}

}  // namespace blink
