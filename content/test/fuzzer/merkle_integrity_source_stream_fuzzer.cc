// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/merkle_integrity_source_stream.h"  // nogncheck

#include <fuzzer/FuzzedDataProvider.h>

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/fuzzed_source_stream.h"

// Fuzzer for MerkleIntegritySourceStream
//
// |data| contains a header prefix, and then is used to build a
// FuzzedSourceStream.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  std::string header = data_provider.ConsumeRandomLengthString(256);

  net::TestCompletionCallback callback;
  auto fuzzed_source_stream =
      std::make_unique<net::FuzzedSourceStream>(&data_provider);
  auto mi_stream = std::make_unique<content::MerkleIntegritySourceStream>(
      header, std::move(fuzzed_source_stream));
  while (true) {
    size_t read_size = data_provider.ConsumeIntegralInRange(1, 1024);
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(read_size);
    int result = mi_stream->Read(io_buffer.get(), io_buffer->size(),
                                 callback.callback());
    // Releasing the pointer to IOBuffer immediately is more likely to lead to a
    // use-after-free.
    io_buffer = nullptr;
    if (callback.GetResult(result) <= 0)
      break;
  }

  return 0;
}
