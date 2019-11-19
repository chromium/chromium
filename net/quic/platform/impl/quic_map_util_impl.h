// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_MAP_UTIL_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_MAP_UTIL_IMPL_H_

#include "base/stl_util.h"

namespace quic {

template <class Collection, class Key>
bool QuicContainsKeyImpl(const Collection& collection, const Key& key) {
  return base::Contains(collection, key);
}

template <typename Collection, typename Value>
bool QuicContainsValueImpl(const Collection& collection, const Value& value) {
  return base::Contains(collection, value);
}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_MAP_UTIL_IMPL_H_
