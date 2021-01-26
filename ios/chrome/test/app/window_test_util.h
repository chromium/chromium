
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_WINDOW_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_WINDOW_TEST_UTIL_H_

#include "base/compiler_specific.h"

namespace web {
class WebState;
}

namespace chrome_test_util {

// Gets current active WebState, in window with given number.
web::WebState* GetCurrentWebStateForWindowWithNumber(int windowNumber);

}  // namespace chrome_test_util

#endif
