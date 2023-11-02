// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/shell/test/shell_apitest.h"

namespace extensions {

using FeedbackPrivateApiTest = ShellApiTest;

IN_PROC_BROWSER_TEST_F(FeedbackPrivateApiTest, Basic) {
  EXPECT_TRUE(RunAppTest("api_test/feedback_private/basic")) << message_;
}

}  // namespace extensions
