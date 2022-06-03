// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

using UsbManualApiTest = extensions::ExtensionApiTest;

// TODO(1020591): The win7 bots do not seem to recognize the MANUAL_ prefix,
// so we explicitly disable this test.
#if defined(OS_WIN)
#define MAYBE_MANUAL_ListInterfaces DISABLED_MANUAL_ListInterfaces
#else
#define MAYBE_MANUAL_ListInterfaces MANUAL_ListInterfaces
#endif
IN_PROC_BROWSER_TEST_F(UsbManualApiTest, MAYBE_MANUAL_ListInterfaces) {
  extensions::PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  extensions::PermissionsRequestFunction::SetAutoConfirmForTests(true);
  ASSERT_TRUE(RunExtensionTest("usb_manual/list_interfaces"));
}
