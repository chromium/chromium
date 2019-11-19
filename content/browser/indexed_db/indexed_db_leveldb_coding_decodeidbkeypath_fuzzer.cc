// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece key_path_str_piece(reinterpret_cast<const char*>(data),
                                       size);
  blink::IndexedDBKeyPath indexed_db_key_path;
  ignore_result(
      content::DecodeIDBKeyPath(&key_path_str_piece, &indexed_db_key_path));

  // Ensure that encoding |indexed_db_key_path| produces the same result.
  std::string result;
  content::EncodeIDBKeyPath(indexed_db_key_path, &result);
  assert(base::StringPiece(result) == key_path_str_piece);
  return 0;
}
