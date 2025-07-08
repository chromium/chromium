// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string_view>

#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view input(reinterpret_cast<const char*>(data), size);
  std::string_view parsed_input(input);
  blink::IndexedDBKey key = content::indexed_db::DecodeIDBKey(&parsed_input);

  // If any prefix of the input decoded into a valid key, re-encode it, then
  // ensure that the encoding matches the original prefix that got decoded.
  if (key.IsValid()) {
    std::string result;
    content::indexed_db::EncodeIDBKey(key, &result);
    // DecodeIDBKey() leaves the unparsed suffix in parsed_input, so strip that
    // much off the end of input before comparing.
    input.remove_suffix(parsed_input.size());
    CHECK_EQ(std::string_view(result), input);
  }

  return 0;
}
