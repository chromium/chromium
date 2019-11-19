// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

using blink::IndexedDBKey;
using blink::mojom::IDBKeyType;

// IDBKeyType has 7 possible values, so the lower 3 bits of |data| are used to
// determine the IDBKeyType to return.
IDBKeyType GetIDBKeyType(uint8_t data) {
  auto enum_mask = data & 0x7;
  switch (enum_mask) {
    case 0:
      return IDBKeyType::Invalid;
    case 1:
      return IDBKeyType::Array;
    case 2:
      return IDBKeyType::Binary;
    case 3:
      return IDBKeyType::String;
    case 4:
      return IDBKeyType::Date;
    case 5:
      return IDBKeyType::Number;
    case 6:
      return IDBKeyType::None;
    case 7:
      return IDBKeyType::Min;
    default:
      return IDBKeyType::Invalid;
  }
}

// Parse |fuzzed_data| to create an IndexedDBKey. This method takes uses the
// first byte to determine the type of key to create. The remaining bytes in
// |fuzzed_data| will be consumed differently depending on the type of key.
IndexedDBKey CreateKey(FuzzedDataProvider* fuzzed_data) {
  // If there is no more data to use, return a |None| type key.
  if (fuzzed_data->remaining_bytes() < 1)
    return IndexedDBKey(IDBKeyType::None);

  auto key_type = GetIDBKeyType(fuzzed_data->ConsumeIntegral<uint8_t>());

  switch (key_type) {
    case IDBKeyType::Array: {
      // Recursively create and add keys to |key_array| until there are no more
      // bytes to consume. Then, create the final key to return with this array.
      IndexedDBKey::KeyArray key_array;
      while (fuzzed_data->remaining_bytes() > 0) {
        key_array.push_back(CreateKey(fuzzed_data));
      }
      return IndexedDBKey(key_array);
    }
    // For keys of type |Binary| and |String|, consume sizeof(size_t) bytes to
    // determine the maximum length of the string to create.
    case IDBKeyType::Binary: {
      if (fuzzed_data->remaining_bytes() < 1)
        return IndexedDBKey("");
      auto str_size = fuzzed_data->ConsumeIntegral<size_t>();
      return IndexedDBKey(fuzzed_data->ConsumeBytesAsString(str_size));
    }
    case IDBKeyType::String: {
      if (fuzzed_data->remaining_bytes() < 1)
        return IndexedDBKey(base::UTF8ToUTF16(std::string()));
      auto str_size = fuzzed_data->ConsumeIntegral<size_t>();
      base::string16 data_str =
          base::UTF8ToUTF16(fuzzed_data->ConsumeBytesAsString(str_size));
      return IndexedDBKey(data_str);
    }
    case IDBKeyType::Date:
    case IDBKeyType::Number: {
      return IndexedDBKey(fuzzed_data->ConsumeFloatingPoint<double>(),
                          key_type);
    }
    case IDBKeyType::Invalid:
    case IDBKeyType::None:
    case IDBKeyType::Min:
    default:
      return IndexedDBKey(key_type);
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);
  auto key = CreateKey(&fuzzed_data);
  std::string result;
  content::EncodeIDBKey(key, &result);

  // Ensure that |result| can be decoded back into the original key.
  auto decoded_key = std::make_unique<IndexedDBKey>();
  auto result_str_piece = base::StringPiece(result);
  ignore_result(content::DecodeIDBKey(&result_str_piece, &decoded_key));
  assert(decoded_key->Equals(key));
  return 0;
}
