// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_SOURCE_STRING_H_
#define MEDIA_FORMATS_HLS_SOURCE_STRING_H_

#include <cstdint>
#include <string_view>

#include "base/types/pass_key.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"

namespace media::hls {

struct SourceLineIterator;
class VariableDictionary;
class ResolvedSourceString;

namespace subtle {

// This structure represents contents of a single line in an HLS manifest, not
// including the line ending. This may be the entire line, or a substring of the
// line (clipped at either/both ends).
template <typename Self>
class MEDIA_EXPORT SourceStringBase {
 public:
  static Self CreateForTesting(std::string_view str);
  static Self CreateForTesting(size_t line,
                               size_t column,
                               std::string_view str);

  // Returns the 1-based line index of this SourceString within the manifest.
  size_t Line() const { return line_; }

  // Returns the 1-based index of the first character of this SourceString from
  // the start of the line within the manifest.
  size_t Column() const { return column_; }

  // Returns the contents of this SourceString. This will never include line-end
  // characters.
  std::string_view Str() const { return str_; }

  bool Empty() const { return str_.empty(); }

  size_t Size() const { return str_.size(); }

  Self Substr(size_t pos = 0, size_t count = std::string_view::npos) const;

  // Consumes this string up to the given count, which may be longer than this
  // string. Returns the substring that was consumed.
  Self Consume(size_t count = std::string_view::npos);

  // Finds the first occurrence of the given character, and returns the
  // substring prefixing that character. The prefix and character are consumed
  // from this string. If the given character does not appear anywhere in this
  // string, the entire string is consumed and returned.
  Self ConsumeDelimiter(char c);

  // Trims whitespace from the start of this SourceString. The only tolerated
  // "whitespace" characters are space (' ') and tab ('\t'). Page break ('\f')
  // is not tolerated, and carriage return ('\r') and line-feed ('\n') should
  // never appear in `SourceString`.
  void TrimStart();

  // Returns whether this string contains variable substitutions, i.e. is
  // different from the original source.
  bool ContainsSubstitutions() const;

 protected:
  SourceStringBase(size_t line, size_t column, std::string_view str);

 private:
  size_t line_;
  size_t column_;
  std::string_view str_;
};

}  // namespace subtle

// A `SourceString` is a slice of the original manifest string that may contain
// unresolved variable references.
class MEDIA_EXPORT SourceString final
    : public subtle::SourceStringBase<SourceString> {
 public:
  // Only `SourceLineIterator` may create `SourceString`s.
  static SourceString Create(base::PassKey<SourceLineIterator>,
                             size_t line,
                             std::string_view str) {
    return SourceString(line, 1, str);
  }

  // Produces a `ResolvedSourceString` by bypassing variable substitution.
  // This is useful for passing strings that must not contain variables to
  // functions consuming strings that may or may not have contained variable
  // references.
  ResolvedSourceString SkipVariableSubstitution() const;

  bool ContainsSubstitutions() const { return false; }

 private:
  friend SourceStringBase;
  SourceString(size_t line, size_t column, std::string_view str);
};

// A `ResolvedSourceString` is a string slice that has either undergone or
// skipped variable substitution, and may differ from the original source.
class MEDIA_EXPORT ResolvedSourceString final
    : public subtle::SourceStringBase<ResolvedSourceString> {
 public:
  enum class SubstitutionState {
    kNoSubstitutions,
    kContainsSubstitutions,
  };

  // Only `VariableDictionary` or `SourceString` may create
  // `ResolvedSourceString`s.
  static ResolvedSourceString Create(base::PassKey<VariableDictionary>,
                                     size_t line,
                                     size_t column,
                                     std::string_view str,
                                     SubstitutionState substitution_state) {
    return ResolvedSourceString(line, column, str, substitution_state);
  }
  static ResolvedSourceString Create(base::PassKey<SourceString>,
                                     size_t line,
                                     size_t column,
                                     std::string_view str) {
    return ResolvedSourceString(line, column, str,
                                SubstitutionState::kNoSubstitutions);
  }

  bool ContainsSubstitutions() const {
    return substitution_state_ == SubstitutionState::kContainsSubstitutions;
  }

 private:
  friend SourceStringBase;
  ResolvedSourceString(size_t line,
                       size_t column,
                       std::string_view str,
                       SubstitutionState substitution_state =
                           SubstitutionState::kNoSubstitutions);

  SubstitutionState substitution_state_;
};

// Exposes a line-based iteration API over the source text of an HLS manifest.
struct MEDIA_EXPORT SourceLineIterator {
  explicit SourceLineIterator(std::string_view source);

  // Moves this SourceLineIterator to the next line, and returns the contents of
  // the current line. Returns `ParseStatusCode::kInvalidEOL` if invalid line
  // endings were found, or `ParseStatusCode::kReachedEOF` if no further lines
  // exist in the manifest.
  ParseStatus::Or<SourceString> Next();

  size_t CurrentLineForTesting() const { return current_line_; }
  std::string_view SourceForTesting() const { return source_; }

 private:
  size_t current_line_ = 1;
  std::string_view source_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_SOURCE_STRING_H_
