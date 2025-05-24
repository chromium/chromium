// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"

namespace blink {

IndexedDBKeyRange::IndexedDBKeyRange() = default;

IndexedDBKeyRange::IndexedDBKeyRange(IndexedDBKey lower,
                                     IndexedDBKey upper,
                                     bool lower_open,
                                     bool upper_open)
    : lower_(std::move(lower)),
      upper_(std::move(upper)),
      lower_open_(lower_open),
      upper_open_(upper_open) {}

IndexedDBKeyRange::~IndexedDBKeyRange() = default;

IndexedDBKeyRange::IndexedDBKeyRange(IndexedDBKeyRange&& other) = default;
IndexedDBKeyRange& IndexedDBKeyRange::operator=(IndexedDBKeyRange&& other) =
    default;

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

IndexedDBKey IndexedDBKeyRange::TakeOnlyKey() && {
  CHECK(IsOnlyKey());
  return std::move(lower_);
}

}  // namespace blink
