// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <tuple>

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view key_str_view(reinterpret_cast<const char*>(data), size);
  auto indexed_db_key = std::make_unique<blink::IndexedDBKey>();
  if (content::indexed_db::DecodeIDBKey(&key_str_view, &indexed_db_key)) {
    // Ensure that encoding |indexed_db_key| produces the same result.
    std::string result;
    content::indexed_db::EncodeIDBKey(*indexed_db_key, &result);
    assert(std::string_view(result) == key_str_view);
  }

  return 0;
}
