// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_record_array.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_record.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"

namespace blink {

IDBRecordArray::IDBRecordArray() = default;

IDBRecordArray::~IDBRecordArray() = default;

IDBRecordArray::IDBRecordArray(IDBRecordArray&& source) = default;

IDBRecordArray& IDBRecordArray::operator=(IDBRecordArray&& source) = default;

v8::Local<v8::Value> IDBRecordArray::ToV8(ScriptState* script_state,
                                          IDBRecordArray source) {
  // Each `IDBRecord` must have a primary key, a value and index.
  // `IDBObjectStore::getAllRecords()` does not populate `index_keys`.
  CHECK_EQ(source.primary_keys.size(), source.values.size());
  CHECK(source.index_keys.size() == source.primary_keys.size() ||
        source.index_keys.empty());

  const wtf_size_t record_count = source.primary_keys.size();

  HeapVector<Member<IDBRecord>> records;
  records.ReserveInitialCapacity(record_count);

  for (wtf_size_t i = 0; i < record_count; ++i) {
    std::unique_ptr<IDBKey> index_key;
    if (!source.index_keys.empty()) {
      index_key = std::move(source.index_keys[i]);
    }
    records.emplace_back(MakeGarbageCollected<IDBRecord>(
        std::move(source.primary_keys[i]), std::move(source.values[i]),
        std::move(index_key)));
  }
  return ToV8Traits<IDLSequence<IDBRecord>>::ToV8(script_state,
                                                  std::move(records));
}

void IDBRecordArray::clear() {
  primary_keys.clear();
  values.clear();
  index_keys.clear();
}

}  // namespace blink
