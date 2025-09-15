// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string_view>

#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Old encoding scheme.
  {
    std::string_view input(reinterpret_cast<const char*>(data), size);
    std::string_view parsed_input(input);
    blink::IndexedDBKey key = content::indexed_db::DecodeIDBKey(&parsed_input);

    // If any prefix of the input decoded into a valid key, re-encode it, then
    // ensure that the encoding matches the original prefix that got decoded.
    if (key.IsValid()) {
      std::string result;
      content::indexed_db::EncodeIDBKey(key, &result);
      // DecodeIDBKey() leaves the unparsed suffix in parsed_input, so strip
      // that much off the end of input before comparing.
      input.remove_suffix(parsed_input.size());

      // Avoid `CHECK_EQ()` since values can be multi-line and that confuses
      // stack trace parsers in fuzzing infra. Print the values on a different
      // line instead.
      CHECK(result == input) << "\nResult: " << result << "\nInput: " << input;
    }
  }

  // New (sortable) encoding scheme.
  {
    std::string_view input(reinterpret_cast<const char*>(data), size);
    blink::IndexedDBKey key = content::indexed_db::DecodeSortableIDBKey(input);

    // Unlike the older encoding, the input has to be exactly decodeable (rather
    // than just a prefix being decodeable).
    if (key.IsValid()) {
      std::string result = content::indexed_db::EncodeSortableIDBKey(key);

      // Avoid `CHECK_EQ()` since values can be multi-line and that confuses
      // stack trace parsers in fuzzing infra. Print the values on a different
      // line instead.
      CHECK(result == input) << "\nResult: " << result << "\nInput: " << input
                             << "\nKey: " << key.DebugString();
    }
  }

  return 0;
}
