// Copyright 2021 The Chromium Authors
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
class VariableDictionary;

// Type representing the resolution state for a `SourceString`.
// As there is only one state here (unresolved), this struct is empty.
struct SourceStringState {};

// Type containing the resolution state for a `ResolvedSourceString`.
struct ResolvedSourceStringState {
  // Whether this string has undergone variable substitution and has
  // substitutions applied to the original source.
  bool contains_substitutions;
};

template <typename T>
class GenericSourceString;

// A `SourceString` is a slice of the original manifest string that may contain
// unresolved variable references.
using SourceString = GenericSourceString<SourceStringState>;

// A `ResolvedSourceString` is a string slice that has either undergone or
// skipped variable substitution, and may differ from the original source.
using ResolvedSourceString = GenericSourceString<ResolvedSourceStringState>;

// This structure represents contents of a single line in an HLS manifest, not
// including the line ending. This may be the entire line, or a substring of the
// line (clipped at either/both ends).
template <typename ResolutionState>
class MEDIA_EXPORT GenericSourceString {
 public:
  static GenericSourceString Create(base::PassKey<SourceLineIterator>,
                                    size_t line,
                                    base::StringPiece str);
  static GenericSourceString Create(base::PassKey<VariableDictionary>,
                                    size_t line,
                                    size_t column,
                                    base::StringPiece str,
                                    ResolutionState resolution_state);
  static GenericSourceString CreateForTesting(base::StringPiece str);
  static GenericSourceString CreateForTesting(size_t line,
                                              size_t column,
                                              base::StringPiece str);
  static GenericSourceString CreateForTesting(size_t line,
                                              size_t column,
                                              base::StringPiece str,
                                              ResolutionState resolution_state);

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

  GenericSourceString Substr(size_t pos = 0,
                             size_t count = base::StringPiece::npos) const;

  // Consumes this string up to the given count, which may be longer than this
  // string. Returns the substring that was consumed.
  GenericSourceString Consume(size_t count = base::StringPiece::npos);

  // Finds the first occurrence of the given character, and returns the
  // substring prefixing that character. The prefix and character are consumed
  // from this string. If the given character does not appear anywhere in this
  // string, the entire string is consumed and returned.
  GenericSourceString ConsumeDelimiter(char c);

  // Produces a `ResolvedSourceString` by bypassing variable substitution.
  // This is useful for passing strings that must not contain variables to
  // functions consuming strings that may or may not have contained variable
  // references.
  ResolvedSourceString SkipVariableSubstitution() const;

  // Trims whitespace from the start of this SourceString. The only tolerated
  // "whitespace" characters are space (' ') and tab ('\t'). Page break ('\f')
  // is not tolerated, and carriage return ('\r') and line-feed ('\n') should
  // never appear in `SourceString`.
  void TrimStart();

  // Returns whether this string contains variable substitutions, i.e. is
  // different from the original source.
  bool ContainsSubstitutions() const;

 private:
  template <typename>
  friend class GenericSourceString;

  GenericSourceString(size_t line,
                      size_t column,
                      base::StringPiece str,
                      ResolutionState resolution_state);

  size_t line_;
  size_t column_;
  base::StringPiece str_;
  ResolutionState resolution_state_;
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
