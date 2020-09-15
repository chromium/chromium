// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_UNORDERED_CONTAINERS_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_UNORDERED_CONTAINERS_IMPL_H_

#include <unordered_map>

#include "third_party/abseil-cpp/absl/container/node_hash_map.h"

namespace quiche {

// The default hasher used by hash tables.
template <typename Key>
using QuicheDefaultHasherImpl = absl::Hash<Key>;

template <typename Key, typename Value, typename Hash, typename Eq>
using QuicheUnorderedMapImpl = absl::node_hash_map<Key, Value, Hash, Eq>;

}  // namespace quiche

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_UNORDERED_CONTAINERS_IMPL_H_
