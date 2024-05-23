// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/url_request/view_cache_helper.h"

#include <algorithm>
#include <utility>

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"

namespace net {

// static
void ViewCacheHelper::HexDump(const char *buf, size_t buf_len,
                              std::string* result) {
  const size_t kMaxRows = 16;
  int offset = 0;

  const unsigned char *p;
  while (buf_len) {
    base::StringAppendF(result, "%08x: ", offset);
    offset += kMaxRows;

    p = (const unsigned char *) buf;

    size_t i;
    size_t row_max = std::min(kMaxRows, buf_len);

    // print hex codes:
    for (i = 0; i < row_max; ++i)
      base::StringAppendF(result, "%02x ", *p++);
    for (i = row_max; i < kMaxRows; ++i)
      result->append("   ");
    result->append(" ");

    // print ASCII glyphs if possible:
    p = (const unsigned char *) buf;
    for (i = 0; i < row_max; ++i, ++p) {
      if (*p < 0x7F && *p > 0x1F) {
        base::AppendEscapedCharForHTML(*p, result);
      } else {
        result->push_back('.');
      }
    }

    result->push_back('\n');

    buf += row_max;
    buf_len -= row_max;
  }
}

}  // namespace net.
