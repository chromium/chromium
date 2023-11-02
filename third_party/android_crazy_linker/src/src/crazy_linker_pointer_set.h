// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_POINTER_SET_H
#define CRAZY_LINKER_POINTER_SET_H

#include "crazy_linker_util.h"

namespace crazy {

// A small container for pointer values (addresses).
class PointerSet {
 public:
  PointerSet() = default;

  // Add a new value to the set.
  bool Add(const void* item);

  // Remove value |item| from the set, if needed. Returns true if the value
  // was previously in the set, false otherwise.
  bool Remove(const void* item);

  // Returns true iff the set contains |item|, false otherwise.
  bool Has(const void* item) const;

  // Return a reference to the values in the set, only for testing.
  const Vector<const void*>& GetValuesForTesting() const { return items_; }

 private:
  // TECHNICAL NOTE: The current implementation uses a simple sorted array,
  // and thus should perform well for sets of a few hundred items, when
  // insertions and removals are pretty rare, but lookups need to be fast.
  Vector<const void*> items_;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_POINTER_SET_H
