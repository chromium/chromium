// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
#define CONTENT_BROWSER_AFTER_STARTUP_TASK_UTILS_H_

#include "content/common/content_export.h"

namespace content {

// An indirection to
// ContentBrowserClient::SetBrowserStartupIsCompleteForTesting() as that can
// only be called from the content implementation and including content/test in
// the content implementation is a whole other can of worms. TODO(gab): Clean
// this up when AfterStartupTasks go away in favor of ThreadPool.
void CONTENT_EXPORT SetBrowserStartupIsCompleteForTesting();

}  // namespace content

#endif  // CONTENT_BROWSER_AFTER_STARTUP_TASK_UTILS_H_
