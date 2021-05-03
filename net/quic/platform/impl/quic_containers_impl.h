// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_CONTAINERS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_CONTAINERS_IMPL_H_

#include <memory>
#include <vector>

#include "net/third_party/quiche/src/common/quiche_linked_hash_map.h"
#include "third_party/abseil-cpp/absl/container/btree_set.h"

namespace quic {

// The default hasher used by hash tables.
template <typename Key>
using QuicDefaultHasherImpl = absl::Hash<Key>;

template <typename Key, typename Value, typename Hash>
using QuicLinkedHashMapImpl = quiche::QuicheLinkedHashMap<Key, Value, Hash>;

// TODO(wub): Switch to absl::InlinedVector once it is allowed.
template <typename T, size_t N, typename A = std::allocator<T>>
using QuicInlinedVectorImpl = std::vector<T, A>;

template <typename Key, typename Compare, typename Rep>
using QuicOrderedSetImpl = absl::btree_set<Key, Compare>;

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_CONTAINERS_IMPL_H_
