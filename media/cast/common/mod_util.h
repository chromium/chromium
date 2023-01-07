// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_MOD_UTIL_H_
#define MEDIA_CAST_COMMON_MOD_UTIL_H_

#include "base/check.h"

namespace media {
namespace cast {

// MAP is a map<uint??, ...> where the unsigned integer is
// assumed to wrap around, but only a small range is used at a time.
// Return the oldest entry in the map.
template<class MAP>
typename MAP::iterator ModMapOldest(MAP* map) {
  typename MAP::iterator ret = map->begin();
  if (ret != map->end()) {
    typename MAP::key_type lower_quarter = 0;
    lower_quarter--;
    lower_quarter >>= 1;
    if (ret->first < lower_quarter) {
      typename MAP::iterator tmp = map->upper_bound(lower_quarter * 3);
      if (tmp != map->end())
        ret = tmp;
    }
  }
  return ret;
}

// MAP is a map<uint??, ...> where the unsigned integer is
// assumed to wrap around, but only a small range is used at a time.
// Returns the previous entry in the map.
template<class MAP>
typename MAP::iterator ModMapPrevious(MAP* map, typename MAP::iterator i) {
  DCHECK(!map->empty());
  typename MAP::iterator ret = i;
  if (i == map->begin()) {
    ret = map->end();
  }
  ret--;
  if (i == ret)
    return map->end();
  if ((i->first - ret->first) > ((typename MAP::key_type(0) - 1)) >> 1)
    return map->end();
  return ret;
}

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_COMMON_MOD_UTIL_H_
