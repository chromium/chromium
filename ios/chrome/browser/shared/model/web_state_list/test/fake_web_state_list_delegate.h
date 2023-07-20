// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_FAKE_WEB_STATE_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_FAKE_WEB_STATE_LIST_DELEGATE_H_

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"

// Fake WebStateList delegate for tests.
class FakeWebStateListDelegate : public WebStateListDelegate {
 public:
  FakeWebStateListDelegate();

  FakeWebStateListDelegate(const FakeWebStateListDelegate&) = delete;
  FakeWebStateListDelegate& operator=(const FakeWebStateListDelegate&) = delete;

  ~FakeWebStateListDelegate() override;

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TEST_FAKE_WEB_STATE_LIST_DELEGATE_H_
