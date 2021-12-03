// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "media/formats/hls/items.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {

bool IsSubstring(base::StringPiece sub, base::StringPiece base) {
  return base.data() <= sub.data() &&
         base.data() + base.size() >= sub.data() + sub.size();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Create a StringPiece from the given input
  base::StringPiece manifest(reinterpret_cast<const char*>(data), size);
  size_t line_number = 1;

  while (true) {
    const auto old_manifest = manifest;
    const auto old_line_number = line_number;
    auto result = media::hls::GetNextLineItem(&manifest, &line_number);

    if (result.has_error()) {
      // Ensure that this was an error this function is expected to return
      CHECK(result.code() == media::hls::ParseStatusCode::kReachedEOF ||
            result.code() == media::hls::ParseStatusCode::kInvalidEOL);

      // Ensure that `manifest` is still a substring of the original manifest
      CHECK(IsSubstring(manifest, old_manifest));
      break;
    }

    auto value = std::move(result).value();
    base::StringPiece item_content;
    size_t item_line_number;
    static_assert(
        absl::variant_size<media::hls::GetNextLineItemResult>::value == 2, "");
    if (auto* tag = absl::get_if<media::hls::TagItem>(&value)) {
      item_content = tag->content;
      item_line_number = tag->line_number;

      // Ensure the tag kind returned was valid
      CHECK(tag->kind >= media::hls::TagKind::kUnknown &&
            tag->kind <= media::hls::TagKind::kMaxValue);
    } else {
      auto uri = absl::get<media::hls::UriItem>(std::move(value));
      item_content = uri.text;
      item_line_number = uri.line_number;
    }

    // Ensure that the line number associated with this item is between the
    // original line number and the updated line number
    CHECK(item_line_number >= old_line_number &&
          item_line_number < line_number);

    // Ensure that `item_content` is a substring of the original manifest
    CHECK(IsSubstring(item_content, old_manifest));

    // Ensure that `manifest` is a substring of the original manifest
    CHECK(IsSubstring(manifest, old_manifest));

    // Ensure that `item_content` is NOT a substring of the updated manifest
    CHECK(!IsSubstring(item_content, manifest));
  }

  return 0;
}
