// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_PARSE_RESULT_H_
#define MEDIA_FORMATS_MP4_PARSE_RESULT_H_

namespace media {
namespace mp4 {

enum class ParseResult {
  kOk,            // Parsing was successful.
  kError,         // The data is invalid (usually unrecoverable).
  kNeedMoreData,  // More data is required to parse successfully.
};

// Evaluate |expr| once. If the result is not ParseResult::kOk, (early) return
// it from the containing function.
#define RCHECK_OK_PARSE_RESULT(expr)                      \
  do {                                                    \
    ::media::mp4::ParseResult result = (expr);            \
    if (result == ::media::mp4::ParseResult::kError)      \
      DLOG(ERROR) << "Failure while parsing MP4: " #expr; \
    if (result != ::media::mp4::ParseResult::kOk)         \
      return result;                                      \
  } while (0)

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_PARSE_RESULT_H_
