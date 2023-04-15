// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_
#define IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_

#include "base/task/single_thread_task_runner.h"
#include "ios/web/public/test/web_test_suite.h"

// Test suite for unit tests.
class IOSChromeUnitTestSuite : public web::WebTestSuite {
 public:
  IOSChromeUnitTestSuite(int argc, char** argv);

  IOSChromeUnitTestSuite(const IOSChromeUnitTestSuite&) = delete;
  IOSChromeUnitTestSuite& operator=(const IOSChromeUnitTestSuite&) = delete;

  ~IOSChromeUnitTestSuite() override;

  // web::WebTestSuite overrides:
  void Initialize() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> action_task_runner_;
};

#endif  // IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_
