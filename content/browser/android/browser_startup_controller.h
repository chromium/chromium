// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BROWSER_STARTUP_CONTROLLER_H_
#define CONTENT_BROWSER_ANDROID_BROWSER_STARTUP_CONTROLLER_H_

namespace content {

void BrowserStartupComplete(int result);
bool ShouldStartGpuProcessOnBrowserStartup();
void MinimalBrowserStartupComplete();

}  // namespace content
#endif  // CONTENT_BROWSER_ANDROID_BROWSER_STARTUP_CONTROLLER_H_
