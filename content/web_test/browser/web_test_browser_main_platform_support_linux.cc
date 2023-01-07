// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_browser_main_platform_support.h"

#include "third_party/test_fonts/fontconfig/fontconfig_util_linux.h"

namespace content {

namespace {
void SetupFonts() {
  test_fonts::SetUpFontconfig();
}
}  // namespace

void WebTestBrowserPlatformInitialize() {
  SetupFonts();
}

}  // namespace content
