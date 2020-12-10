// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_

#include "ios/web/public/test/fakes/fake_web_state_observer.h"

// TODO(crbug.com/688063): Remove this file after updating all clients to import
// fake_web_state_observer.h and use FakeWebStateObserver class.

namespace web {
using TestWebStateObserver = FakeWebStateObserver;
}

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_
