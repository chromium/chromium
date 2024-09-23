// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/indexed_db/indexed_db_value.h"

#include "base/check.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

// static
blink::mojom::IDBValuePtr IndexedDBValue::ConvertAndEraseValue(
    IndexedDBValue* value) {
  auto mojo_value = blink::mojom::IDBValue::New();
  if (!value->empty()) {
    mojo_value->bits = std::move(value->bits);
  }
  IndexedDBExternalObject::ConvertToMojo(value->external_objects,
                                         &mojo_value->external_objects);
  return mojo_value;
}

IndexedDBValue::IndexedDBValue() = default;
IndexedDBValue::IndexedDBValue(
    const std::string& input_bits,
    const std::vector<IndexedDBExternalObject>& external_objects)
    : bits(input_bits.begin(), input_bits.end()),
      external_objects(external_objects) {
  DCHECK(external_objects.empty() || input_bits.size());
}
IndexedDBValue::IndexedDBValue(const IndexedDBValue& other) = default;
IndexedDBValue::~IndexedDBValue() = default;
IndexedDBValue& IndexedDBValue::operator=(const IndexedDBValue& other) =
    default;

}  // namespace content::indexed_db
