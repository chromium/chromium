// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/source_string.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"
#include "media/formats/hls/parse_status.h"

namespace media::hls {

SourceString SourceString::Create(base::PassKey<SourceLineIterator>,
                                  size_t line,
                                  base::StringPiece str) {
  return SourceString(line, 1, str);
}

SourceString SourceString::CreateForTesting(base::StringPiece str) {
  return SourceString::CreateForTesting(1, 1, str);
}

SourceString SourceString::CreateForTesting(size_t line,
                                            size_t column,
                                            base::StringPiece str) {
  return SourceString(line, column, str);
}

SourceString::SourceString(size_t line, size_t column, base::StringPiece str)
    : line_(line), column_(column), str_(str) {}

SourceString SourceString::Substr(size_t pos, size_t count) const {
  const auto column = column_ + pos;
  return SourceString(line_, column, str_.substr(pos, count));
}

SourceString SourceString::Consume(size_t count) {
  count = std::min(count, str_.size());

  auto consumed = Substr(0, count);
  *this = Substr(count);

  return consumed;
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

}  // namespace media::hls
