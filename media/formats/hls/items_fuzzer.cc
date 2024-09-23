// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/hls/items.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {

bool IsSubstring(std::string_view sub, std::string_view base) {
  return sub.empty() || (base.data() <= sub.data() &&
                         base.data() + base.size() >= sub.data() + sub.size());
}

std::optional<media::hls::SourceString> GetItemContent(
    media::hls::TagItem tag) {
  // Ensure the tag kind returned was valid
  if (tag.GetName()) {
    auto kind = media::hls::GetTagKind(*tag.GetName());
    CHECK(kind >= media::hls::TagKind::kMinValue &&
          kind <= media::hls::TagKind::kMaxValue);
  }

  return tag.GetContent();
}

std::optional<media::hls::SourceString> GetItemContent(
    media::hls::UriItem uri) {
  return uri.content;
}

size_t GetItemLineNumber(media::hls::TagItem tag) {
  return tag.GetLineNumber();
}

size_t GetItemLineNumber(media::hls::UriItem uri) {
  return uri.content.Line();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Create a StringPiece from the given input
  const std::string_view source(reinterpret_cast<const char*>(data), size);
  media::hls::SourceLineIterator iterator{source};

  while (true) {
    const auto prev_iterator = iterator;
    auto result = media::hls::GetNextLineItem(&iterator);

    if (!result.has_value()) {
      // Ensure that this was an error this function is expected to return
      CHECK(result == media::hls::ParseStatusCode::kReachedEOF ||
            result == media::hls::ParseStatusCode::kInvalidEOL);

      // Ensure that `source` is still a substring of the previous source
      CHECK(IsSubstring(iterator.SourceForTesting(),
                        prev_iterator.SourceForTesting()));
      CHECK(iterator.CurrentLineForTesting() >=
            prev_iterator.CurrentLineForTesting());
      break;
    }

    auto value = std::move(result).value();
    auto content = absl::visit([](auto x) { return GetItemContent(x); }, value);
    auto line_number =
        absl::visit([](auto x) { return GetItemLineNumber(x); }, value);

    // Ensure that the line number associated with this item is between the
    // original line number and the updated line number
    CHECK(line_number >= prev_iterator.CurrentLineForTesting() &&
          line_number < iterator.CurrentLineForTesting());

    // Ensure that the content associated with this item is a substring of the
    // previous iterator
    if (content) {
      CHECK(IsSubstring(content->Str(), prev_iterator.SourceForTesting()));
    }

    // Ensure that the updated iterator contains a substring of the previous
    // iterator
    CHECK(IsSubstring(iterator.SourceForTesting(),
                      prev_iterator.SourceForTesting()));

    // Ensure that the content associated with this item is NOT a substring of
    // the updated iterator, if the content has any associated data.
    if (content && content->Size()) {
      CHECK(!IsSubstring(content->Str(), iterator.SourceForTesting()));
    }
  }

  return 0;
}
