// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_TASK_OBSERVER_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_TASK_OBSERVER_UTIL_H_

namespace web {
class WebState;

namespace test {

// Blocks until both known NSRunLoop-based and known message-loop-based
// background tasks have completed
void WaitForBackgroundTasks();

// Blocks until `web_state` navigation and background tasks are
// completed. Returns false when timed out.
bool WaitUntilLoaded(WebState* web_state);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_TASK_OBSERVER_UTIL_H_
