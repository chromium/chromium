// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/brotli_source_stream.h"

#include <fuzzer/FuzzedDataProvider.h>

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/filter/fuzzed_source_stream.h"
#include "net/filter/source_stream.h"

// Fuzzer for BrotliSourceStream.
//
// |data| is used to create a FuzzedSourceStream.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::TestCompletionCallback callback;
  FuzzedDataProvider data_provider(data, size);

  const bool is_shared_dictionary = data_provider.ConsumeBool();
  std::unique_ptr<net::SourceStream> brotli_stream;

  if (is_shared_dictionary) {
    const std::string dictionary = data_provider.ConsumeRandomLengthString();
    scoped_refptr<net::IOBuffer> dictionary_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(dictionary);
    auto fuzzed_source_stream =
        std::make_unique<net::FuzzedSourceStream>(&data_provider);
    brotli_stream = net::CreateBrotliSourceStreamWithDictionary(
        std::move(fuzzed_source_stream), dictionary_buffer, dictionary.size());
  } else {
    auto fuzzed_source_stream =
        std::make_unique<net::FuzzedSourceStream>(&data_provider);
    brotli_stream =
        net::CreateBrotliSourceStream(std::move(fuzzed_source_stream));
  }

  while (true) {
    scoped_refptr<net::IOBufferWithSize> io_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(64);
    int result = brotli_stream->Read(io_buffer.get(), io_buffer->size(),
                                     callback.callback());
    // Releasing the pointer to IOBuffer immediately is more likely to lead to a
    // use-after-free.
    io_buffer = nullptr;
    if (callback.GetResult(result) <= 0)
      break;
  }

  return 0;
}
