// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/webfonts_histogram.h"

#include "base/strings/string_piece.h"
#include "net/disk_cache/blockfile/histogram_macros.h"

namespace {

// Tests if the substring of str that begins at pos starts with substr. If so,
// returns true and advances pos by the length of substr.
bool Consume(const std::string& str,
             base::StringPiece substr,
             std::string::size_type* pos) {
  if (!str.compare(*pos, substr.length(), substr.data())) {
    *pos += substr.length();
    return true;
  }
  return false;
}

const char kRoboto[] = "roboto";
const char kOpenSans[] = "opensans";

const char kRobotoHistogramName[] = "WebFont.HttpCacheStatus_roboto";
const char kOpenSansHistogramName[] = "WebFont.HttpCacheStatus_opensans";
const char kOthersHistogramName[] = "WebFont.HttpCacheStatus_others";

void RecordCacheEvent(net::HttpResponseInfo::CacheEntryStatus cache_status,
                      const std::string& histogram_name) {
  CACHE_HISTOGRAM_ENUMERATION(
      histogram_name, cache_status,
      net::HttpResponseInfo::CacheEntryStatus::ENTRY_MAX);
}

}  // namespace

namespace net {
namespace web_fonts_histogram {

// Check if |key| is a URL for a font resource of Google Fonts.
// If so, record the WebFont.HttpCacheStatus histogram suffixed by "roboto",
// "opensans" or "others".
void MaybeRecordCacheStatus(HttpResponseInfo::CacheEntryStatus cache_status,
                            const std::string& key) {
  std::string::size_type pos = 0;
  if (Consume(key, "https://", &pos) || Consume(key, "http://", &pos)) {
    if (Consume(key, "themes.googleusercontent.com/static/fonts/", &pos) ||
        Consume(key, "ssl.gstatic.com/fonts/", &pos) ||
        Consume(key, "fonts.gstatic.com/s/", &pos)) {
      if (Consume(key, kRoboto, &pos)) {
        RecordCacheEvent(cache_status, kRobotoHistogramName);
      } else if (Consume(key, kOpenSans, &pos)) {
        RecordCacheEvent(cache_status, kOpenSansHistogramName);
      } else {
        RecordCacheEvent(cache_status, kOthersHistogramName);
      }
    }
  }
}

}  // namespace web_fonts_histogram
}  // namespace net
