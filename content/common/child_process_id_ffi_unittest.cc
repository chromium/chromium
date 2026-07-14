// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "content/public/common/child_process_id.h"

namespace content {

// Note: This file is named `child_process_id_ffi_unittest.cc` rather than
// `child_process_id_unittest.cc` to avoid a GN/Ninja object file name clash
// with the Android JNI test in
// `//content/common/android/child_process_id_unittest.cc`.
//
// Enforce that C++ `content::ChildProcessId` matches the `i32` layout and
// triviality of the Rust-side `ChildProcessId`. If C++ `ChildProcessId` ever
// changes size (e.g., to `IdType64`), passing it across FFI without updating
// Rust will break the FFI interface.
static_assert(sizeof(ChildProcessId) == sizeof(int32_t), "");
static_assert(alignof(ChildProcessId) == alignof(int32_t), "");
static_assert(std::is_trivially_destructible_v<ChildProcessId>, "");
static_assert(std::is_trivially_copy_constructible_v<ChildProcessId>, "");
static_assert(std::is_trivially_copy_assignable_v<ChildProcessId>, "");
static_assert(std::is_trivially_move_constructible_v<ChildProcessId>, "");
static_assert(std::is_trivially_move_assignable_v<ChildProcessId>, "");

}  // namespace content
