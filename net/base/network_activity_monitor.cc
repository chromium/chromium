// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_activity_monitor.h"

#include <atomic>
#include <type_traits>

namespace net {
namespace activity_monitor {

namespace {

std::atomic<uint64_t> g_bytes_received{0};

static_assert(
    std::is_trivially_constructible<decltype(g_bytes_received)>::value,
    "g_bytes_received generates a static initializer");
static_assert(std::is_trivially_destructible<decltype(g_bytes_received)>::value,
              "g_bytes_received generates a static destructor");

}  // namespace

void IncrementBytesReceived(uint64_t bytes_received) {
  // std::memory_order_relaxed is used because no other operation on
  // |bytes_received_| depends on memory operations that happened before this
  // increment.
  g_bytes_received.fetch_add(bytes_received, std::memory_order_relaxed);
}

uint64_t GetBytesReceived() {
  return g_bytes_received.load(std::memory_order_relaxed);
}

void ResetBytesReceivedForTesting() {
  g_bytes_received = 0;
}

}  // namespace activity_monitor
}  // namespace net
