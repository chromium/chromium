// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/dump_cache/dump_cache_helper.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"

// static
void DumpCacheHelper::HexDump(base::span<const uint8_t> buf,
                              std::string* result) {
  const size_t kMaxRows = 16;
  int offset = 0;

  while (!buf.empty()) {
    base::StringAppendF(result, "%08x: ", offset);
    offset += kMaxRows;

    size_t i;
    size_t row_max = std::min(kMaxRows, buf.size());

    // print hex codes:
    for (i = 0; i < row_max; ++i) {
      base::StringAppendF(result, "%02x ", buf[i]);
    }
    for (i = row_max; i < kMaxRows; ++i) {
      result->append("   ");
    }
    result->append(" ");

    // print ASCII glyphs if possible:
    for (i = 0; i < row_max; ++i) {
      uint8_t val = buf[i];
      if (val < 0x7F && val > 0x1F) {
        base::AppendEscapedCharForHTML(val, result);
      } else {
        result->push_back('.');
      }
    }

    result->push_back('\n');

    buf.take_first(row_max);
  }
}
