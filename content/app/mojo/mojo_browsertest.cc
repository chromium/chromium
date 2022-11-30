// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/c/system/core.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MojoTest : public ContentBrowserTest {
 public:
  MojoTest() {}

  MojoTest(const MojoTest&) = delete;
  MojoTest& operator=(const MojoTest&) = delete;

 protected:
  bool IsMojoInitialized() {
    // Check |MojoGetTimeTicksNow()| is accessible.
    MojoGetTimeTicksNow();
    return true;
  }
};

// Placeholder test to confirm we are initializing Mojo.
IN_PROC_BROWSER_TEST_F(MojoTest, Init) {
  EXPECT_TRUE(IsMojoInitialized());
}

}  // namespace content
