// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_FILTER_SOURCE_STREAM_TEST_UTIL_H_
#define NET_FILTER_FILTER_SOURCE_STREAM_TEST_UTIL_H_

#include <stdint.h>

#include <string_view>
#include <vector>

namespace net {

// Compress `source` using gzip. If `gzip_framing` is true, header will be
// added.
std::vector<uint8_t> CompressGzip(std::string_view source,
                                  bool gzip_framing = true);

}  // namespace net

#endif  // NET_FILTER_FILTER_SOURCE_STREAM_TEST_UTIL_H_
