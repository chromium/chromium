// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_preview_element_info_internal.h"

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

using CWVPreviewElementInfoTest = PlatformTest;

// Tests CWVPreviewElementInfoTest initialization.
TEST_F(CWVPreviewElementInfoTest, Initialization) {
  NSURL* const linkURL = [NSURL URLWithString:@"https://chromium.test"];
  CWVPreviewElementInfo* element =
      [[CWVPreviewElementInfo alloc] initWithLinkURL:linkURL];
  EXPECT_NSEQ(linkURL, element.linkURL);
}

}  // namespace ios_web_view
