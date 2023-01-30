// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Part of generating an artificial Rust crash for testing purposes.
// We call through this C++ function to ensure we can cope with mixed
// language stacks.

#include "third_party/blink/common/rust_crash/src/lib.rs.h"

namespace blink {

void EnterCppForRustCrash() {
  reenter_rust();
}

}  // namespace blink
