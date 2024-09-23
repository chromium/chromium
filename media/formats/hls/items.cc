// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/items.h"

#include <optional>
#include <string_view>

#include "base/strings/string_util.h"
#include "media/formats/hls/parse_status.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media::hls {

namespace {

// Determines tag type, which may be known or unknown.
TagItem GetTagItem(SourceString tag) {
  // Tags that include content have the name separated by a colon.
  const auto colon_index = tag.Str().find_first_of(':');

  // Extract name and content
  const auto name_str = tag.Substr(0, colon_index);

  std::optional<SourceString> content;
  if (colon_index != std::string_view::npos) {
    content = tag.Substr(colon_index + 1);
  }

  auto name = ParseTagName(name_str.Str());
  if (name.has_value()) {
    return content ? TagItem::Create(*name, *content)
                   : TagItem::CreateEmpty(*name, tag.Line());
  }

  return TagItem::CreateUnknown(name_str);
}

}  // namespace

std::string_view TagItem::GetNameStr() {
  if (!name_.has_value()) {
    return content_or_name_->Str();
  }

  return TagNameToString(*name_);
}

ParseStatus::Or<GetNextLineItemResult> GetNextLineItem(
    SourceLineIterator* src) {
  while (true) {
    auto result = src->Next();
    if (!result.has_value()) {
      // Forward error to caller
      return std::move(result).error();
    }

    auto line = std::move(result).value();

    // Empty lines are permitted, but ignored
    if (line.Str().empty()) {
      continue;
    }

    // Tags and comments start with '#'
    if (line.Str().front() == '#') {
      line.Consume(1);

      // All tags begin with "EXT", otherwise it's a comment.
      if (base::StartsWith(line.Str(), "EXT")) {
        return GetNextLineItemResult{GetTagItem(line)};
      }

      continue;
    }

    // If not empty, tag, or comment, it must be a URI.
    // This line may contain leading, trailing, or interior whitespace,
    // but that's the URI parser's responsibility.
    return GetNextLineItemResult{UriItem{.content = line}};
  }
}

}  // namespace media::hls
