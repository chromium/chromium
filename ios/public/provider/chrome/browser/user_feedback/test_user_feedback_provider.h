// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_TEST_USER_FEEDBACK_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_TEST_USER_FEEDBACK_PROVIDER_H_

#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"

class TestUserFeedbackProvider : public UserFeedbackProvider {
 public:
  TestUserFeedbackProvider();

  TestUserFeedbackProvider(const TestUserFeedbackProvider&) = delete;
  TestUserFeedbackProvider& operator=(const TestUserFeedbackProvider&) = delete;

  ~TestUserFeedbackProvider() override;

  // UserFeedbackProvider implementation.
  void Synchronize() override;

  // Helper methods for tests.
  bool SynchronizeCalled() const;
  void ResetSynchronizeCalled();

 private:
  bool synchronize_called_ = false;
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_TEST_USER_FEEDBACK_PROVIDER_H_
