// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browsing_instance_id.h"

#include <type_traits>

namespace content {

// Enforce that C++ `content::BrowsingInstanceId` matches the `i32` layout and
// triviality of the Rust-side `BrowsingInstanceId`. If C++ `BrowsingInstanceId`
// ever changes size (e.g., to `IdType64`), passing it across FFI without
// updating Rust will break the FFI interface.
static_assert(sizeof(BrowsingInstanceId) == sizeof(int32_t), "");
static_assert(alignof(BrowsingInstanceId) == alignof(int32_t), "");
static_assert(std::is_trivially_destructible_v<BrowsingInstanceId>, "");
static_assert(std::is_trivially_copy_constructible_v<BrowsingInstanceId>, "");
static_assert(std::is_trivially_copy_assignable_v<BrowsingInstanceId>, "");
static_assert(std::is_trivially_move_constructible_v<BrowsingInstanceId>, "");
static_assert(std::is_trivially_move_assignable_v<BrowsingInstanceId>, "");

}  // namespace content
