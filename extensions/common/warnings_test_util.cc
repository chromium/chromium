// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/warnings_test_util.h"

#include "base/memory/scoped_refptr.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::warnings_test_util {

bool HasInstallWarning(scoped_refptr<Extension> extension,
                       const std::string& expected_message) {
  return base::ranges::count_if(
             extension->install_warnings(),
             [expected_message](const InstallWarning& warning) {
               return warning.message == expected_message;
             }) == 1;
}

}  // namespace extensions::warnings_test_util
