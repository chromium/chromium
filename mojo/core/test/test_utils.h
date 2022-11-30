// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_TEST_TEST_UTILS_H_
#define MOJO_CORE_TEST_TEST_UTILS_H_

#include <stddef.h>
#include <stdio.h>

#include "base/files/scoped_file.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {
namespace core {
namespace test {

// Gets a (scoped) |PlatformHandle| from the given (scoped) |FILE|.
PlatformHandle PlatformHandleFromFILE(base::ScopedFILE fp);

// Gets a (scoped) |FILE| from a (scoped) |PlatformHandle|.
base::ScopedFILE FILEFromPlatformHandle(PlatformHandle h, const char* mode);

}  // namespace test
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_TEST_TEST_UTILS_H_
