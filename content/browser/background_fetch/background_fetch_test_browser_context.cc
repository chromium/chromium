// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_browser_context.h"

#include <utility>

namespace content {

BackgroundFetchTestBrowserContext::BackgroundFetchTestBrowserContext() =
    default;

BackgroundFetchTestBrowserContext::~BackgroundFetchTestBrowserContext() =
    default;

MockBackgroundFetchDelegate*
BackgroundFetchTestBrowserContext::GetBackgroundFetchDelegate() {
  if (!delegate_)
    delegate_ = std::make_unique<MockBackgroundFetchDelegate>();

  return delegate_.get();
}

}  // namespace content
