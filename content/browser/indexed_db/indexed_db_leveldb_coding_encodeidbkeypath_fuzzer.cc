// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"

using blink::IndexedDBKeyPath;

IndexedDBKeyPath GetKeyPath(FuzzedDataProvider* fuzzed_data) {
  // If there is no more data to use, return an empty key path.
  if (fuzzed_data->remaining_bytes() < 1)
    return IndexedDBKeyPath();

  // Consume sizeof(size_t) bytes to determine the size of the vector of strings
  // to use for the IndexedDBKeyPath.
  auto vector_size = fuzzed_data->ConsumeIntegral<size_t>();

  if (vector_size == 0) {
    return IndexedDBKeyPath();
  } else if (vector_size == 1) {
    // Consume all of |fuzzed_data| to create an IndexedDBKeyPath.
    auto str16 =
        base::UTF8ToUTF16(fuzzed_data->ConsumeRemainingBytesAsString());
    return IndexedDBKeyPath(str16);
  }

  // Create and add string16s to |paths| until |vector_size| is reached or the
  // end of |data| is reached.
  std::vector<base::string16> paths;
  for (size_t i = 0; i < vector_size && fuzzed_data->remaining_bytes() > 0;
       ++i) {
    // Consume sizeof(size_t) bytes to determine the size of the string to
    // create.
    size_t str_size = fuzzed_data->ConsumeIntegral<size_t>();

    auto str16 = base::UTF8ToUTF16(fuzzed_data->ConsumeBytesAsString(str_size));
    paths.push_back(str16);
  }
  return IndexedDBKeyPath(paths);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);
  IndexedDBKeyPath key_path = GetKeyPath(&fuzzed_data);
  std::string result;
  content::EncodeIDBKeyPath(key_path, &result);

  // Ensure that |result| can be decoded back into the original key path.
  IndexedDBKeyPath decoded_key_path;
  auto result_str_piece = base::StringPiece(result);
  ignore_result(
      content::DecodeIDBKeyPath(&result_str_piece, &decoded_key_path));
  assert(decoded_key_path == key_path);
  return 0;
}
