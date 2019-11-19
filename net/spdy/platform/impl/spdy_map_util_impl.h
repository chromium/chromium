// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_PLATFORM_IMPL_SPDY_MAP_UTIL_IMPL_H_
#define NET_SPDY_PLATFORM_IMPL_SPDY_MAP_UTIL_IMPL_H_

#include "base/stl_util.h"

namespace spdy {

template <class Collection, class Key>
bool SpdyContainsKeyImpl(const Collection& collection, const Key& key) {
  return base::Contains(collection, key);
}

}  // namespace spdy

#endif  // NET_SPDY_PLATFORM_IMPL_SPDY_MAP_UTIL_IMPL_H_
