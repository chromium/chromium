// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/gzip_source_stream.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <memory>

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/fuzzed_source_stream.h"

// Fuzzer for GzipSourceStream.
//
// |data| is used to create a FuzzedSourceStream.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  auto fuzzed_source_stream =
      std::make_unique<net::FuzzedSourceStream>(&data_provider);

  // Bound the total number of reads. Gzip has a maximum compression ratio of
  // 1032x. While, strictly speaking, linear, this means the fuzzer will often
  // get stuck. Bound the number of reads rather than the size of the output
  // because lots of 1-byte chunks is also a problem.
  const size_t kMaxReads = 10 * 1024;

  const net::SourceStream::SourceType kGzipTypes[] = {
      net::SourceStream::TYPE_GZIP, net::SourceStream::TYPE_DEFLATE};
  net::SourceStream::SourceType type =
      data_provider.PickValueInArray(kGzipTypes);
  std::unique_ptr<net::GzipSourceStream> gzip_stream =
      net::GzipSourceStream::Create(std::move(fuzzed_source_stream), type);
  size_t num_reads = 0;
  while (num_reads < kMaxReads) {
    scoped_refptr<net::IOBufferWithSize> io_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(64);
    net::TestCompletionCallback callback;
    int result = gzip_stream->Read(io_buffer.get(), io_buffer->size(),
                                   callback.callback());
    ++num_reads;

    // Releasing the pointer to IOBuffer immediately is more likely to lead to a
    // use-after-free.
    io_buffer = nullptr;
    if (callback.GetResult(result) <= 0)
      break;
  }

  return 0;
}
