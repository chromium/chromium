// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_CLIENT_H_

#import "ios/web/public/test/fakes/fake_web_client.h"

// TODO(crbug.com/688063): Remove this file after updating all clients to import
// fake_web_client.h and use FakeWebClient class.

namespace web {
using TestWebClient = FakeWebClient;
}

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_CLIENT_H_
