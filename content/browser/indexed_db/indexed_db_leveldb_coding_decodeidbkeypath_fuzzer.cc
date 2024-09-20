// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <tuple>

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view key_path_str_view(reinterpret_cast<const char*>(data), size);
  blink::IndexedDBKeyPath indexed_db_key_path;
  std::ignore = content::indexed_db::DecodeIDBKeyPath(&key_path_str_view,
                                                      &indexed_db_key_path);

  // Ensure that encoding |indexed_db_key_path| produces the same result.
  std::string result;
  content::indexed_db::EncodeIDBKeyPath(indexed_db_key_path, &result);
  assert(std::string_view(result) == key_path_str_view);
  return 0;
}
