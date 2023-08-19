// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_BROTLI_SOURCE_STREAM_H_
#define NET_FILTER_BROTLI_SOURCE_STREAM_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/source_stream.h"

namespace net {

NET_EXPORT_PRIVATE std::unique_ptr<FilterSourceStream> CreateBrotliSourceStream(
    std::unique_ptr<SourceStream> upstream);

NET_EXPORT_PRIVATE std::unique_ptr<FilterSourceStream>
CreateBrotliSourceStreamWithDictionary(std::unique_ptr<SourceStream> upstream,
                                       scoped_refptr<IOBuffer> dictionary,
                                       size_t dictionary_size);

}  // namespace net

#endif  // NET_FILTER_BROTLI_SOURCE_STREAM_H_
