// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_SECURITY_UTIL_WIN_H_
#define MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_SECURITY_UTIL_WIN_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/win/windows_types.h"

namespace mojo {

using FileHandleSecurityErrorCallback = base::RepeatingCallback<bool()>;

// This function DCHECKs if `handle` is to a writeable file that can be mapped
// executable. If so, this is a security risk. Does nothing in non-DCHECK
// builds.
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
void DcheckIfFileHandleIsUnsafe(HANDLE handle);

// Sets a callback for testing that will be called before DCHECKing inside
// DcheckIfFileHandleIsUnsafe because of an insecure handle. If the callback has
// been set, and returns true, then the error has been successfully handled and
// a DCHECK will not happen, otherwise DcheckIfFileHandleIsUnsafe will DCHECK as
// normal.
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
void SetUnsafeFileHandleCallbackForTesting(
    FileHandleSecurityErrorCallback callback);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_PLATFORM_HANDLE_SECURITY_UTIL_WIN_H_
