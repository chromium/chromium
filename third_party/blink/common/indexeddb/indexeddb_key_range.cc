// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"

namespace blink {

IndexedDBKeyRange::IndexedDBKeyRange() = default;

IndexedDBKeyRange::IndexedDBKeyRange(const blink::IndexedDBKey& lower,
                                     const blink::IndexedDBKey& upper,
                                     bool lower_open,
                                     bool upper_open)
    : lower_(lower),
      upper_(upper),
      lower_open_(lower_open),
      upper_open_(upper_open) {}

IndexedDBKeyRange::IndexedDBKeyRange(const blink::IndexedDBKey& key)
    : lower_(key), upper_(key) {}

IndexedDBKeyRange::IndexedDBKeyRange(const IndexedDBKeyRange& other) = default;
IndexedDBKeyRange::~IndexedDBKeyRange() = default;
IndexedDBKeyRange& IndexedDBKeyRange::operator=(
    const IndexedDBKeyRange& other) = default;

bool IndexedDBKeyRange::IsOnlyKey() const {
  if (lower_open_ || upper_open_)
    return false;
  if (IsEmpty())
    return false;

  return lower_.Equals(upper_);
}

bool IndexedDBKeyRange::IsEmpty() const {
  return !lower_.IsValid() && !upper_.IsValid();
}

}  // namespace blink
