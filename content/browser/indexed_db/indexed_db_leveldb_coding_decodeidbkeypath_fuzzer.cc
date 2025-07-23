// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <tuple>

#include "base/check.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view parsed_input(reinterpret_cast<const char*>(data), size);
  std::string_view input(parsed_input);
  blink::IndexedDBKeyPath indexed_db_key_path;
  if (content::indexed_db::DecodeIDBKeyPath(&parsed_input,
                                            &indexed_db_key_path)) {
    // Ensure that encoding |indexed_db_key_path| produces the same result.
    std::string result;
    content::indexed_db::EncodeIDBKeyPath(indexed_db_key_path, &result);

    input.remove_suffix(parsed_input.size());

    // There is an old style of encoding where the type is not encoded, and the
    // keypath is just a string. In this case, when we re-encode, the output
    // *will* encode the type, and this format takes 6 extra bytes.
    if (input.size() >= 3 && input[0] != 0 && input[1] != 0) {
      CHECK_EQ(indexed_db_key_path.type(),
               blink::mojom::IDBKeyPathType::String);
      CHECK_EQ(result.size(), input.size() + 6);

      std::string_view reencoded(result);
      blink::IndexedDBKeyPath redecoded;
      CHECK(content::indexed_db::DecodeIDBKeyPath(&reencoded, &redecoded));
      CHECK_EQ(redecoded.string(), indexed_db_key_path.string());
      return 0;
    }

    CHECK_EQ(std::string_view(result), input);
  }
  return 0;
}
