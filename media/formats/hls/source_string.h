// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_SOURCE_STRING_H_
#define MEDIA_FORMATS_HLS_SOURCE_STRING_H_

#include <cstdint>
#include "base/strings/string_piece.h"
#include "base/types/pass_key.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"

namespace media::hls {

struct SourceLineIterator;

// This structure represents contents of a single line in an HLS manifest, not
// including the line ending. This may be the entire line, or a substring of the
// line (clipped at either/both ends).
struct MEDIA_EXPORT SourceString {
  static SourceString Create(base::PassKey<SourceLineIterator>,
                             size_t line,
                             base::StringPiece str);
  static SourceString CreateForTesting(base::StringPiece str);
  static SourceString CreateForTesting(size_t line,
                                       size_t column,
                                       base::StringPiece str);

  // Returns the 1-based line index of this SourceString within the manifest.
  size_t Line() const { return line_; }

  // Returns the 1-based index of the first character of this SourceString from
  // the start of the line within the manifest.
  size_t Column() const { return column_; }

  // Returns the contents of this SourceString. This will never include line-end
  // characters.
  base::StringPiece Str() const { return str_; }

  bool Empty() const { return str_.empty(); }

  size_t Size() const { return str_.size(); }

  SourceString Substr(size_t pos = 0,
                      size_t count = base::StringPiece::npos) const;

  // Consumes this string up to the given count, which may be longer than this
  // string. Returns the substring that was consumed.
  SourceString Consume(size_t count = base::StringPiece::npos);

 private:
  SourceString(size_t line, size_t column, base::StringPiece str);

  size_t line_;
  size_t column_;
  base::StringPiece str_;
};

// Exposes a line-based iteration API over the source text of an HLS manifest.
struct MEDIA_EXPORT SourceLineIterator {
  explicit SourceLineIterator(base::StringPiece source);

  // Moves this SourceLineIterator to the next line, and returns the contents of
  // the current line. Returns `ParseStatusCode::kInvalidEOL` if invalid line
  // endings were found, or `ParseStatusCode::kReachedEOF` if no further lines
  // exist in the manifest.
  ParseStatus::Or<SourceString> Next();

  size_t CurrentLineForTesting() const { return current_line_; }
  base::StringPiece SourceForTesting() const { return source_; }

 private:
  size_t current_line_;
  base::StringPiece source_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_SOURCE_STRING_H_
