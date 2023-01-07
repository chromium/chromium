// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/brotli_source_stream.h"

namespace net {

std::unique_ptr<FilterSourceStream> CreateBrotliSourceStream(
    std::unique_ptr<SourceStream> previous) {
  return nullptr;
}

}  // namespace net
