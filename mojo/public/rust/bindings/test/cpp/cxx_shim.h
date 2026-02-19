// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_
#define MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_

#include "mojo/public/c/system/types.h"

namespace bindings_unittests::mojom {

void BindPlusSevenMathService(MojoHandle handle);
void TestRemoteFromCpp(MojoHandle handle);

}  // namespace bindings_unittests::mojom

#endif  // MOJO_PUBLIC_RUST_BINDINGS_TEST_CPP_CXX_SHIM_H_
