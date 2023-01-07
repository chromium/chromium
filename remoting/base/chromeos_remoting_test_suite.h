// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CHROMEOS_REMOTING_TEST_SUITE_H_
#define REMOTING_BASE_CHROMEOS_REMOTING_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace remoting {

// Allows for testing scenarios which require localized resources (e.g tests for
// It2MeConfirmationDialogChromeOS).
class ChromeOSRemotingTestSuite : public base::TestSuite {
 public:
  ChromeOSRemotingTestSuite(int argc, char** argv);

  ChromeOSRemotingTestSuite(const ChromeOSRemotingTestSuite&) = delete;
  ChromeOSRemotingTestSuite& operator=(const ChromeOSRemotingTestSuite&) =
      delete;

  ~ChromeOSRemotingTestSuite() override;

 protected:
  void Initialize() override;
  void Shutdown() override;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CHROMEOS_REMOTING_TEST_SUITE_H_
