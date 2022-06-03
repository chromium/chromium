// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/user_feedback/test_user_feedback_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestUserFeedbackProvider::TestUserFeedbackProvider() {}

TestUserFeedbackProvider::~TestUserFeedbackProvider() {}

void TestUserFeedbackProvider::Synchronize() {
  synchronize_called_ = true;
}

bool TestUserFeedbackProvider::SynchronizeCalled() const {
  return synchronize_called_;
}

void TestUserFeedbackProvider::ResetSynchronizeCalled() {
  synchronize_called_ = false;
}
