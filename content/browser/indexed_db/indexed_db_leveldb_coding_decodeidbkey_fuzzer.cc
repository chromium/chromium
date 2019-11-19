// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece key_str_piece(reinterpret_cast<const char*>(data), size);
  auto indexed_db_key = std::make_unique<blink::IndexedDBKey>();
  ignore_result(content::DecodeIDBKey(&key_str_piece, &indexed_db_key));

  // Ensure that encoding |indexed_db_key| produces the same result.
  std::string result;
  content::EncodeIDBKey(*indexed_db_key, &result);
  assert(base::StringPiece(result) == key_str_piece);
  return 0;
}
