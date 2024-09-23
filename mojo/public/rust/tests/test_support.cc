// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Similar to support.cc, this patches over changes from the old Mojo SDK to
// Mojo now. Test-specific helpers are here.

#include "mojo/public/rust/tests/test_support.h"

#include <cstdint>

#include "base/command_line.h"
#include "mojo/core/embedder/embedder.h"

void InitializeMojoEmbedder(std::uint32_t argc, const char* const* argv) {
  // Some mojo internals check command line flags, so we must initialize it
  // here.
  base::CommandLine::Init(argc, argv);
  mojo::core::Init();
}
