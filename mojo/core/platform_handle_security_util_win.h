// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PLATFORM_HANDLE_SECURITY_UTIL_WIN_H_
#define MOJO_CORE_PLATFORM_HANDLE_SECURITY_UTIL_WIN_H_

#include "base/win/windows_types.h"

namespace mojo::core {

// This function DCHECKs if `handle` is to a writeable file that can be mapped
// executable. If so, this is a security risk. Does nothing in non-DCHECK
// builds.
void DcheckIfFileHandleIsUnsafe(HANDLE handle);

}  // namespace mojo::core

#endif  // MOJO_CORE_PLATFORM_HANDLE_SECURITY_UTIL_WIN_H_
