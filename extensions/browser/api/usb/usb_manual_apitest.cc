// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

using UsbManualApiTest = extensions::ExtensionApiTest;

// TODO(crbug.com/40656552): The win7 bots do not seem to recognize the MANUAL_
// prefix, so we explicitly disable this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MANUAL_ListInterfaces DISABLED_MANUAL_ListInterfaces
#else
#define MAYBE_MANUAL_ListInterfaces MANUAL_ListInterfaces
#endif
IN_PROC_BROWSER_TEST_F(UsbManualApiTest, MAYBE_MANUAL_ListInterfaces) {
  auto dialog_action_reset =
      extensions::PermissionsRequestFunction::SetDialogActionForTests(
          extensions::PermissionsRequestFunction::DialogAction::kAutoConfirm);
  extensions::PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(RunExtensionTest("usb_manual/list_interfaces"));
}
