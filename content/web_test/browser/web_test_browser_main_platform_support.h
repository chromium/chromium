// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_PLATFORM_SUPPORT_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_PLATFORM_SUPPORT_H_

#include "build/build_config.h"

namespace content {

#if BUILDFLAG(IS_WIN)
// Called in the browser process to ensure the system is set up correctly before
// running web tests.
bool WebTestBrowserCheckLayoutSystemDeps();
#endif

// Called in the browser process to initialize anything platform-specific for
// web tests.
void WebTestBrowserPlatformInitialize();

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_MAIN_PLATFORM_SUPPORT_H_
