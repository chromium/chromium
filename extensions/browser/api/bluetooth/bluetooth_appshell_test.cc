// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "extensions/shell/test/shell_apitest.h"

using BluetoothShellApiTest = extensions::ShellApiTest;

// TODO(crbug.com/40741808): this test flakes on Mac ASAN
#if BUILDFLAG(IS_MAC)
#define MAYBE_ApiSanityCheck DISABLED_ApiSanityCheck
#else
#define MAYBE_ApiSanityCheck ApiSanityCheck
#endif
IN_PROC_BROWSER_TEST_F(BluetoothShellApiTest, MAYBE_ApiSanityCheck) {
  ASSERT_TRUE(RunAppTest("api_test/bluetooth"));
}
