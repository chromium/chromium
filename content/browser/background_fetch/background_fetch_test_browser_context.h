// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BROWSER_CONTEXT_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BROWSER_CONTEXT_H_

#include <memory>

#include "content/browser/background_fetch/mock_background_fetch_delegate.h"
#include "content/public/test/test_browser_context.h"

namespace content {

class BackgroundFetchTestBrowserContext : public TestBrowserContext {
 public:
  BackgroundFetchTestBrowserContext();
  ~BackgroundFetchTestBrowserContext() override;

  MockBackgroundFetchDelegate* GetBackgroundFetchDelegate() override;

 private:
  std::unique_ptr<MockBackgroundFetchDelegate> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BROWSER_CONTEXT_H_
