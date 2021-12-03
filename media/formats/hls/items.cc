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

// A non-blank line. This may be a comment, tag, or URI.
struct LineItem {
  size_t number;

  // The content of the line. This does not include the line ending.
  base::StringPiece text;
};

// Constructs a new Line from the given source text. Verifies that line endings
// are respected, and advances `src` and `line_number` to the following line.
ParseStatus::Or<LineItem> GetLineItem(base::StringPiece* src,
                                      size_t* line_number) {
  // Caller must not pass in an empty line
  DCHECK(!src->empty());

  auto line_end = src->find_first_of("\r\n");
  if (line_end == base::StringPiece::npos) {
    return ParseStatusCode::kInvalidEOL;
  }

  auto text = src->substr(0, line_end);
  const auto following = src->substr(line_end);

  // Trim (and validate) newline sequence from the following text
  if (base::StartsWith(following, "\n")) {
    *src = following.substr(1);
  } else if (base::StartsWith(following, "\r\n")) {
    *src = following.substr(2);
  } else {
    return ParseStatusCode::kInvalidEOL;
  }

  return LineItem{.number = (*line_number)++, .text = text};
}

// Attempts to determine tag type, if this line contains a tag.
absl::optional<TagItem> GetTagItem(LineItem line) {
  constexpr base::StringPiece kTagPrefix = "#EXT";

  // If this line does not begin with #EXT prefix, it must be a comment.
  if (!base::StartsWith(line.text, kTagPrefix)) {
    return absl::nullopt;
  }

  auto content = line.text.substr(kTagPrefix.size());

  constexpr std::pair<base::StringPiece, TagKind> kTagKindPrefixes[] = {
      {"M3U", TagKind::kM3u},
      {"-X-VERSION:", TagKind::kXVersion},
      {"INF:", TagKind::kInf},
  };

  for (const auto& tag : kTagKindPrefixes) {
    if (base::StartsWith(content, tag.first)) {
      content = content.substr(tag.first.size());
      return TagItem(tag.second, line.number, content);
    }
  }

  return TagItem(TagKind::kUnknown, line.number, content);
}

}  // namespace

TagItem::TagItem(TagKind kind, size_t line_number, base::StringPiece content)
    : kind(kind), line_number(line_number), content(content) {}

TagItem::~TagItem() = default;
TagItem::TagItem(const TagItem&) = default;
TagItem::TagItem(TagItem&&) = default;
TagItem& TagItem::operator=(const TagItem&) = default;
TagItem& TagItem::operator=(TagItem&&) = default;

UriItem::UriItem(size_t line_number, base::StringPiece text)
    : line_number(line_number), text(text) {}

UriItem::~UriItem() = default;
UriItem::UriItem(const UriItem&) = default;
UriItem::UriItem(UriItem&&) = default;
UriItem& UriItem::operator=(const UriItem&) = default;
UriItem& UriItem::operator=(UriItem&&) = default;

ParseStatus::Or<GetNextLineItemResult> GetNextLineItem(base::StringPiece* src,
                                                       size_t* line_number) {
  while (!src->empty()) {
    auto result = GetLineItem(src, line_number);
    if (result.has_error()) {
      // Forward error to caller
      return std::move(result).error();
    }

    auto line = std::move(result).value();

    // Empty lines are permitted, but ignored
    if (line.text.empty()) {
      continue;
    }

    // Tags and comments start with '#', try to get a tag
    if (line.text.front() == '#') {
      auto tag = GetTagItem(line);
      if (!tag.has_value()) {
        continue;
      }
      return GetNextLineItemResult(std::move(tag).value());
    }

    // If not empty, tag, or comment, it must be a URI.
    // This line may contain leading, trailing, or interior whitespace,
    // but that's the URI parser's responsibility.
    return GetNextLineItemResult(UriItem(line.number, line.text));
  }

  return ParseStatusCode::kReachedEOF;
}

}  // namespace hls
}  // namespace media
