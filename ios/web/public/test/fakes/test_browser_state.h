// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_BROWSER_STATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_BROWSER_STATE_H_

#include "ios/web/public/test/fakes/fake_browser_state.h"

// TODO(crbug.com/688063): Remove this file after updating all clients to import
// fake_browser_state.h and use FakeBrowserState class.

namespace web {
using TestBrowserState = FakeBrowserState;
}

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_BROWSER_STATE_H_
