// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/source_string.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"
#include "media/formats/hls/parse_status.h"

namespace media::hls {

namespace subtle {
// static
template <typename Self>
Self SourceStringBase<Self>::CreateForTesting(base::StringPiece str) {
  return Self(1, 1, str);
}

// static
template <typename Self>
Self SourceStringBase<Self>::CreateForTesting(size_t line,
                                              size_t column,
                                              base::StringPiece str) {
  return Self(line, column, str);
}

template <typename Self>
Self SourceStringBase<Self>::Substr(size_t pos, size_t count) const {
  Self result = static_cast<const Self&>(*this);
  result.column_ = column_ + pos;
  result.str_ = str_.substr(pos, count);
  return result;
}

template <typename Self>
Self SourceStringBase<Self>::Consume(size_t count) {
  count = std::min(count, str_.size());

  auto consumed = Substr(0, count);
  static_cast<Self&>(*this) = Substr(count);

  return consumed;
}

template <typename Self>
Self SourceStringBase<Self>::ConsumeDelimiter(char c) {
  const auto index = Str().find_first_of(c);
  const auto prefix = Consume(index);
  Consume(1);
  return prefix;
}

template <typename Self>
void SourceStringBase<Self>::TrimStart() {
  auto start = Str().find_first_not_of(" \t");
  Consume(start);
}

template <typename Self>
SourceStringBase<Self>::SourceStringBase(size_t line,
                                         size_t column,
                                         base::StringPiece str)
    : line_(line), column_(column), str_(str) {}

}  // namespace subtle

SourceString::SourceString(size_t line, size_t column, base::StringPiece str)
    : SourceStringBase(line, column, str) {}

ResolvedSourceString::ResolvedSourceString(size_t line,
                                           size_t column,
                                           base::StringPiece str,
                                           SubstitutionState substitution_state)
    : SourceStringBase(line, column, str),
      substitution_state_(substitution_state) {}

ResolvedSourceString SourceString::SkipVariableSubstitution() const {
  return ResolvedSourceString::Create(base::PassKey<SourceString>(), Line(),
                                      Column(), Str());
}

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
// `SourceStringBase` with these arguments, allowing us to keep these
// definitions in our .cc without causing linker errors. This also means if
// anyone tries to instantiate a `SourceStringBase` with anything but these
// two specializations they'll most likely get linker errors.
template class MEDIA_EXPORT subtle::SourceStringBase<SourceString>;
template class MEDIA_EXPORT subtle::SourceStringBase<ResolvedSourceString>;

}  // namespace media::hls
