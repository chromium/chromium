// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_value.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

IndexedDBValue::IndexedDBValue() = default;
IndexedDBValue::~IndexedDBValue() = default;

IndexedDBValue::IndexedDBValue(IndexedDBValue&& other) = default;
IndexedDBValue& IndexedDBValue::operator=(IndexedDBValue&& other) = default;

IndexedDBValue IndexedDBValue::Clone() const {
  IndexedDBValue copy;
  copy.bits = mojo_base::BigBuffer(base::span(bits));
  copy.external_objects = external_objects;
  return copy;
}

IndexedDBValue::IndexedDBValue(
    const std::string& input_bits,
    const std::vector<IndexedDBExternalObject>& external_objects)
    : bits(base::as_byte_span(input_bits)), external_objects(external_objects) {
  DCHECK(external_objects.empty() || input_bits.size());
}

}  // namespace content::indexed_db
