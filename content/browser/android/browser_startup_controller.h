// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BROWSER_STARTUP_CONTROLLER_H_
#define CONTENT_BROWSER_ANDROID_BROWSER_STARTUP_CONTROLLER_H_

#include "base/time/time.h"

namespace content {

void BrowserStartupComplete(
    int result,
    base::TimeDelta longest_duration_of_posted_startup_tasks,
    base::TimeDelta total_duration_of_posted_startup_tasks);
bool ShouldStartGpuProcessOnBrowserStartup();
void MinimalBrowserStartupComplete();

}  // namespace content
#endif  // CONTENT_BROWSER_ANDROID_BROWSER_STARTUP_CONTROLLER_H_
