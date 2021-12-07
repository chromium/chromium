// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/items.h"

#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "media/formats/hls/parse_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media {
namespace hls {

namespace {

// Attempts to determine tag type, if this line contains a tag.
absl::optional<TagItem> GetTagItem(SourceString line) {
  constexpr base::StringPiece kTagPrefix = "#EXT";

  // If this line does not begin with #EXT prefix, it must be a comment.
  if (!base::StartsWith(line.Str(), kTagPrefix)) {
    return absl::nullopt;
  }

  auto content = line.Substr(kTagPrefix.size());

  constexpr std::pair<base::StringPiece, TagKind> kTagKindPrefixes[] = {
      {"M3U", TagKind::kM3u},
      {"-X-VERSION:", TagKind::kXVersion},
      {"INF:", TagKind::kInf},
  };

  for (const auto& tag : kTagKindPrefixes) {
    if (base::StartsWith(content.Str(), tag.first)) {
      content = content.Substr(tag.first.size());
      return TagItem(tag.second, content);
    }
  }

  return TagItem(TagKind::kUnknown, content);
}

}  // namespace

TagItem::TagItem(TagKind kind, SourceString content)
    : kind(kind), content(content) {}

TagItem::~TagItem() = default;
TagItem::TagItem(const TagItem&) = default;
TagItem::TagItem(TagItem&&) = default;
TagItem& TagItem::operator=(const TagItem&) = default;
TagItem& TagItem::operator=(TagItem&&) = default;

UriItem::UriItem(SourceString content) : content(content) {}

UriItem::~UriItem() = default;
UriItem::UriItem(const UriItem&) = default;
UriItem::UriItem(UriItem&&) = default;
UriItem& UriItem::operator=(const UriItem&) = default;
UriItem& UriItem::operator=(UriItem&&) = default;

ParseStatus::Or<GetNextLineItemResult> GetNextLineItem(
    SourceLineIterator* src) {
  while (true) {
    auto result = src->Next();
    if (result.has_error()) {
      // Forward error to caller
      return std::move(result).error();
    }

    auto line = std::move(result).value();

    // Empty lines are permitted, but ignored
    if (line.Str().empty()) {
      continue;
    }

    // Tags and comments start with '#', try to get a tag
    if (line.Str().front() == '#') {
      auto tag = GetTagItem(line);
      if (!tag.has_value()) {
        continue;
      }

      return GetNextLineItemResult{std::move(tag).value()};
    }

    // If not empty, tag, or comment, it must be a URI.
    // This line may contain leading, trailing, or interior whitespace,
    // but that's the URI parser's responsibility.
    return GetNextLineItemResult{UriItem(line)};
  }
}

}  // namespace hls
}  // namespace media
