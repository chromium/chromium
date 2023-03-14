// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/ng_shape_cache.h"

namespace blink {

// Hard limit to guard against pathological growth. This number is chosen small
// enough to mitigate risks of memory regression while still making caching
// efficient. It is smaller than the legacy ShapeCache::kMaxSize.
static constexpr unsigned kMaxSize = 2000;

// Maximum number of entries after a cleanup. The value must not be too large
// so that cleanup is fast and not too frequent ; but must also not be too small
// so that we retain enough cached entries for future reuses.
// A value of 0 is interpreted as in the legacy ShapeCache: the hash table is
// just cleared and NGShapeCache::AddSlowCase returns a nullptr.
static constexpr unsigned kMaxSizeAfterCleanup = 3 * kMaxSize / 4;

ShapeCacheEntry* NGShapeCache::AddSlowCase(const String& text,
                                           TextDirection direction) {
  SmallStringKey key(text, direction);
  bool is_new_entry;
  ShapeCacheEntry* new_value;
  {
    SmallStringMap::AddResult add_result =
        small_string_map_.insert(key, ShapeCacheEntry());
    is_new_entry = add_result.is_new_entry;
    new_value = &add_result.stored_value->value;
  }

  if (is_new_entry && small_string_map_.size() > kMaxSize) {
    // Adding this new entry made the table exceed the maximum size so rebuild
    // the cache, respecting the kMaxSizeAfterCleanup constraint.
    if (kMaxSizeAfterCleanup > 0) {
      DCHECK_LE(kMaxSizeAfterCleanup, kMaxSize);
      // First add a slot for the ShapeCacheEntry that is about to be used.
      SmallStringMap preserved_map;
      preserved_map.insert(key, ShapeCacheEntry());
      if (kMaxSizeAfterCleanup > 1) {
        // Next, keep other entries that are still referenced from outside that
        // hash table, with the assumption that such entries are likely to be
        // used again.
        auto end = small_string_map_.end();
        for (auto it = small_string_map_.begin(); it != end; ++it) {
          if (!it->key.IsHashTableDeletedValue() &&
              !it->key.IsHashTableEmptyValue() && it->value) {
            DCHECK(it->value->HasAtLeastOneRef());
            if (!it->value->HasOneRef()) {
              preserved_map.insert(it->key, it->value);
              if (preserved_map.size() == kMaxSizeAfterCleanup) {
                break;
              }
            }
          }
        }
      }
      small_string_map_.swap(preserved_map);

      // Now calculate the pointer to the added value again.
      {
        SmallStringMap::AddResult add_result =
            small_string_map_.insert(key, ShapeCacheEntry());
        DCHECK(!add_result.is_new_entry);
        new_value = &add_result.stored_value->value;
      }
    } else {
      // No need to be fancy: we're just trying to avoid pathological growth.
      small_string_map_.clear();
      return nullptr;
    }
  }

  return new_value;
}

}  // namespace blink
