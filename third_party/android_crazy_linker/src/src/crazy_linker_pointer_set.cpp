// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_pointer_set.h"

namespace crazy {

static SearchResult BinarySearch(const Vector<const void*>& items,
                                 const void* key) {
  auto key_val = reinterpret_cast<uintptr_t>(key);
  size_t min = 0, max = items.GetCount();
  while (min < max) {
    size_t mid = min + ((max - min) >> 1);
    auto mid_val = reinterpret_cast<uintptr_t>(items[mid]);
    if (mid_val == key_val) {
      return {true, mid};
    }
    if (mid_val < key_val)
      min = mid + 1;
    else
      max = mid;
  }
  return {false, min};
}

bool PointerSet::Add(const void* item) {
  SearchResult ret = BinarySearch(items_, item);
  if (ret.found)
    return true;

  items_.InsertAt(ret.pos, item);
  return false;
}

bool PointerSet::Remove(const void* item) {
  SearchResult ret = BinarySearch(items_, item);
  if (!ret.found)
    return false;

  items_.RemoveAt(ret.pos);
  return true;
}

bool PointerSet::Has(const void* item) const {
  SearchResult ret = BinarySearch(items_, item);
  return ret.found;
}

}  // namespace crazy
