// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_LOCAL_STATE_H_
#define IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_LOCAL_STATE_H_

#include "components/prefs/testing_pref_service.h"

// Helper class to temporarily set up a `local_state` in the global
// TestingApplicationContext.
class IOSChromeScopedTestingLocalState {
 public:
  IOSChromeScopedTestingLocalState();

  IOSChromeScopedTestingLocalState(const IOSChromeScopedTestingLocalState&) =
      delete;
  IOSChromeScopedTestingLocalState& operator=(
      const IOSChromeScopedTestingLocalState&) = delete;

  ~IOSChromeScopedTestingLocalState();

 private:
  TestingPrefServiceSimple local_state_;
};

#endif  // IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_LOCAL_STATE_H_
