// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/source_string.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"
#include "media/formats/hls/parse_status.h"

namespace media::hls {

// static
template <>
SourceString SourceString::Create(base::PassKey<SourceLineIterator>,
                                  size_t line,
                                  base::StringPiece str) {
  return SourceString(line, 1, str, {});
}

// static
template <>
ResolvedSourceString ResolvedSourceString::Create(
    base::PassKey<VariableDictionary>,
    size_t line,
    size_t column,
    base::StringPiece str,
    ResolvedSourceStringState resolution_state) {
  return ResolvedSourceString(line, column, str, resolution_state);
}

// static
template <typename ResolutionState>
GenericSourceString<ResolutionState>
GenericSourceString<ResolutionState>::CreateForTesting(base::StringPiece str) {
  return GenericSourceString::CreateForTesting(1, 1, str);
}

// static
template <>
SourceString SourceString::CreateForTesting(size_t line,
                                            size_t column,
                                            base::StringPiece str) {
  return SourceString::CreateForTesting(line, column, str, {});
}

// static
template <>
ResolvedSourceString ResolvedSourceString::CreateForTesting(
    size_t line,
    size_t column,
    base::StringPiece str) {
  return ResolvedSourceString::CreateForTesting(
      line, column, str,
      ResolvedSourceStringState{.contains_substitutions = false});
}

// static
template <typename ResolutionState>
GenericSourceString<ResolutionState>
GenericSourceString<ResolutionState>::CreateForTesting(
    size_t line,
    size_t column,
    base::StringPiece str,
    ResolutionState resolution_state) {
  return GenericSourceString(line, column, str, resolution_state);
}

template <typename ResolutionState>
GenericSourceString<ResolutionState>
GenericSourceString<ResolutionState>::Substr(size_t pos, size_t count) const {
  const auto column = column_ + pos;
  return GenericSourceString(line_, column, str_.substr(pos, count),
                             resolution_state_);
}

template <typename ResolutionState>
GenericSourceString<ResolutionState>
GenericSourceString<ResolutionState>::Consume(size_t count) {
  count = std::min(count, str_.size());

  auto consumed = Substr(0, count);
  *this = Substr(count);

  return consumed;
}

template <typename ResolutionState>
GenericSourceString<ResolutionState>
GenericSourceString<ResolutionState>::ConsumeDelimiter(char c) {
  const auto index = Str().find_first_of(c);
  const auto prefix = Consume(index);
  Consume(1);
  return prefix;
}

template <>
ResolvedSourceString SourceString::SkipVariableSubstitution() const {
  return ResolvedSourceString(
      Line(), Column(), Str(),
      ResolvedSourceStringState{.contains_substitutions = false});
}

template <typename ResolutionState>
void GenericSourceString<ResolutionState>::TrimStart() {
  auto start = Str().find_first_not_of(" \t");
  Consume(start);
}

template <>
bool SourceString::ContainsSubstitutions() const {
  return false;
}

template <>
bool ResolvedSourceString::ContainsSubstitutions() const {
  return resolution_state_.contains_substitutions;
}

template <typename ResolutionState>
GenericSourceString<ResolutionState>::GenericSourceString(
    size_t line,
    size_t column,
    base::StringPiece str,
    ResolutionState resolution_state)
    : line_(line),
      column_(column),
      str_(str),
      resolution_state_(resolution_state) {}

SourceLineIterator::SourceLineIterator(base::StringPiece source)
    : current_line_(1), source_(source) {}

ParseStatus::Or<SourceString> SourceLineIterator::Next() {
  if (source_.empty()) {
    return ParseStatusCode::kReachedEOF;
  }

  const auto line_end = source_.find_first_of("\r\n");
  if (line_end == base::StringPiece::npos) {
    return ParseStatusCode::kInvalidEOL;
  }

  const auto line_content = source_.substr(0, line_end);
  const auto following = source_.substr(line_end);

  // Trim (and validate) newline sequence from the following text
  if (base::StartsWith(following, "\n")) {
    source_ = following.substr(1);
  } else if (base::StartsWith(following, "\r\n")) {
    source_ = following.substr(2);
  } else {
    return ParseStatusCode::kInvalidEOL;
  }

  const auto line_number = current_line_;
  current_line_ += 1;

  return SourceString::Create({}, line_number, line_content);
}

// These forward declarations tell the compiler that we will use
// `GenericSourceString` with these arguments, allowing us to keep these
// definitions in our .cc without causing linker errors. This also means if
// anyone tries to instantiate a `GenericSourceString` with anything but these
// two specializations they'll most likely get linker errors.
template class MEDIA_EXPORT GenericSourceString<SourceStringState>;
template class MEDIA_EXPORT GenericSourceString<ResolvedSourceStringState>;

}  // namespace media::hls
