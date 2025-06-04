// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/preloading_headers.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(PreloadingHeadersTest, IsSecPurposeForPrefetch) {
  EXPECT_TRUE(IsSecPurposeForPrefetch(kSecPurposePrefetchHeaderValue));
  EXPECT_TRUE(
      IsSecPurposeForPrefetch(kSecPurposePrefetchAnonymousClientIpHeaderValue));
  EXPECT_TRUE(IsSecPurposeForPrefetch(kSecPurposePrefetchPrerenderHeaderValue));
  EXPECT_TRUE(
      IsSecPurposeForPrefetch(kSecPurposePrefetchPrerenderPreviewHeaderValue));
  EXPECT_FALSE(IsSecPurposeForPrefetch(std::nullopt));
  EXPECT_FALSE(IsSecPurposeForPrefetch(""));
}

}  // namespace blink
