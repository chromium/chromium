// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_UNORDERED_CONTAINERS_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_UNORDERED_CONTAINERS_IMPL_H_

#include <unordered_map>

namespace quiche {

// The default hasher used by hash tables.
template <typename Key>
using QuicheDefaultHasherImpl = std::hash<Key>;

template <typename Key,
          typename Value,
          typename Hash,
          typename Eq =
              typename std::unordered_map<Key, Value, Hash>::key_equal,
          typename Alloc =
              typename std::unordered_map<Key, Value, Hash>::allocator_type>
using QuicheUnorderedMapImpl = std::unordered_map<Key, Value, Hash, Eq, Alloc>;

}  // namespace quiche

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_UNORDERED_CONTAINERS_IMPL_H_
