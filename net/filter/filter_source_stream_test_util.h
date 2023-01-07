// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_FILTER_SOURCE_STREAM_TEST_UTIL_H_
#define NET_FILTER_FILTER_SOURCE_STREAM_TEST_UTIL_H_

#include <stddef.h>

namespace net {

// Compress |source| with length |source_len| using gzip. Write output into
// |dest|, and output length into |dest_len|. If |gzip_framing| is true, header
// will be added.
void CompressGzip(const char* source,
                  size_t source_len,
                  char* dest,
                  size_t* dest_len,
                  bool gzip_framing);

}  // namespace net

#endif  // NET_FILTER_FILTER_SOURCE_STREAM_TEST_UTIL_H_
