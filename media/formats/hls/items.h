// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_ITEMS_H_
#define MEDIA_FORMATS_HLS_ITEMS_H_

#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/tag_name.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media::hls {

// An 'Item' is a lexical item in an HLS manifest which has been determined to
// have some type based on its context, but has yet been fully parsed,
// validated, or undergone variable substitution.

// An item which has been determined to be of a known or unknown tag type, but
// not a comment.
class MEDIA_EXPORT TagItem {
 public:
  // Helper for representing an unknown tag.
  static TagItem CreateUnknown(SourceString name) {
    return TagItem{std::nullopt, name, name.Line()};
  }

  // Helper for representing a tag with no content.
  static TagItem CreateEmpty(TagName name, size_t line_number) {
    return TagItem{name, std::nullopt, line_number};
  }

  // Helper for representing a tag with content.
  static TagItem Create(TagName name, SourceString content) {
    return TagItem{name, content, content.Line()};
  }

  // Returns the name constant of the tag, if this is a known tag.
  // If this is an unknown tag, returns `std::nullopt`.
  std::optional<TagName> GetName() const { return name_; }

  // Returns the name of the tag as a string.
  std::string_view GetNameStr();

  // Returns the line number this tag appeared on.
  size_t GetLineNumber() const { return line_number_; }

  // Returns the content associated with this tag. If this tag is unknown or has
  // no content, returns `std::nullopt`.
  std::optional<SourceString> GetContent() const {
    return name_ ? content_or_name_ : std::nullopt;
  }

 private:
  TagItem(std::optional<TagName> name,
          std::optional<SourceString> content_or_name,
          size_t line_number)
      : name_(name),
        content_or_name_(content_or_name),
        line_number_(line_number) {}

  std::optional<TagName> name_;
  std::optional<SourceString> content_or_name_;
  size_t line_number_;
};

// A URI. This only corresponds to line-level URIs.
struct UriItem {
  SourceString content;
};

using GetNextLineItemResult = absl::variant<TagItem, UriItem>;

// Returns the next line-level item from the source text. Automatically skips
// empty lines.
MEDIA_EXPORT ParseStatus::Or<GetNextLineItemResult> GetNextLineItem(
    SourceLineIterator* src);

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_ITEMS_H_
