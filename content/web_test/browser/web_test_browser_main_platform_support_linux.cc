// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_browser_main_platform_support.h"

#include "base/test/fontconfig_util_linux.h"

namespace content {

namespace {
void SetupFonts() {
  base::SetUpFontconfig();
}
}  // namespace

void WebTestBrowserPlatformInitialize() {
  SetupFonts();
}

}  // namespace content
