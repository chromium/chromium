// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_COMMON_WARNINGS_TEST_UTIL_H_
#define EXTENSIONS_COMMON_WARNINGS_TEST_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "extensions/common/extension.h"

// Convenience methods for testing InstallWarnings.
namespace extensions::warnings_test_util {

// Checks if a specific warning was set on extension install once.
bool HasInstallWarning(scoped_refptr<Extension> extension,
                       const std::string& expected_message);

}  // namespace extensions::warnings_test_util

#endif  // EXTENSIONS_COMMON_WARNINGS_TEST_UTIL_H_
