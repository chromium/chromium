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

namespace media::hls {

namespace {

template <typename T>
constexpr std::pair<base::StringPiece, TagName> TagNameEntry(
    base::StringPiece prefix,
    T name) {
  return std::make_pair(prefix, ToTagName(name));
}

// Attempts to determine tag type, if this line contains a tag.
absl::optional<TagItem> GetTagItem(SourceString line) {
  constexpr base::StringPiece kTagPrefix = "#EXT";

  // If this line does not begin with #EXT prefix, it must be a comment.
  if (!base::StartsWith(line.Str(), kTagPrefix)) {
    return absl::nullopt;
  }

  auto content = line.Substr(kTagPrefix.size());

  constexpr std::pair<base::StringPiece, TagName> kTagNames[] = {
      TagNameEntry("-X-DEFINE:", CommonTagName::kXDefine),
      TagNameEntry("-X-DISCONTINUITY", MediaPlaylistTagName::kXDiscontinuity),
      TagNameEntry("-X-END-LIST", MediaPlaylistTagName::kXEndList),
      TagNameEntry("-X-GAP", MediaPlaylistTagName::kXGap),
      TagNameEntry("-X-I-FRAMES-ONLY", MediaPlaylistTagName::kXIFramesOnly),
      TagNameEntry("-X-INDEPENDENT-SEGMENTS",
                   CommonTagName::kXIndependentSegments),
      TagNameEntry("-X-VERSION:", CommonTagName::kXVersion),
      TagNameEntry("INF:", MediaPlaylistTagName::kInf),
      TagNameEntry("M3U", CommonTagName::kM3u),
  };

  for (const auto& tag : kTagNames) {
    if (base::StartsWith(content.Str(), tag.first)) {
      content = content.Substr(tag.first.size());
      return TagItem{.name = tag.second, .content = content};
    }
  }

  return TagItem{.name = kUnknownTagName, .content = content};
}

}  // namespace

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
    return GetNextLineItemResult{UriItem{.content = line}};
  }
}

}  // namespace media::hls
