// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TEST_SUITE_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TEST_SUITE_H_

#include "base/test/test_suite.h"
#include "ios/web/public/test/scoped_testing_web_client.h"

namespace web {

class WebTestSuite : public base::TestSuite {
 public:
  WebTestSuite(int argc, char** argv);

  WebTestSuite(const WebTestSuite&) = delete;
  WebTestSuite& operator=(const WebTestSuite&) = delete;

  ~WebTestSuite() override;

 protected:
  // base::TestSuite overrides.
  void Initialize() override;
  void Shutdown() override;

 private:
  // Sets web client on construction and restores the original on destruction.
  ScopedTestingWebClient web_client_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_SUITE_H_
