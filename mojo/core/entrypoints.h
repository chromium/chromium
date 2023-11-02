// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_ENTRYPOINTS_H_
#define MOJO_CORE_ENTRYPOINTS_H_

#include "mojo/core/system_impl_export.h"
#include "mojo/public/c/system/thunks.h"

namespace mojo {
namespace core {

// Initializes the global Core object.
MOJO_SYSTEM_IMPL_EXPORT void InitializeCore();

// Destroys the global Core object.
MOJO_SYSTEM_IMPL_EXPORT void ShutDownCore();

// Returns a MojoSystemThunks2 struct populated with the EDK's implementation
// of each function. This may be used by embedders to populate thunks for
// application loading.
MOJO_SYSTEM_IMPL_EXPORT const MojoSystemThunks2& GetSystemThunks();

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_ENTRYPOINTS_H_
