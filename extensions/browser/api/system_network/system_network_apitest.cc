// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_apitest.h"

namespace {

class SystemNetworkApiTest : public extensions::ShellApiTest {};

}  // namespace

IN_PROC_BROWSER_TEST_F(SystemNetworkApiTest, SystemNetworkExtension) {
  ASSERT_TRUE(RunAppTest("system/network")) << message_;
}
