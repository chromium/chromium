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

TEST(PreloadingHeadersTest, IsSecPurposeForPrerender) {
  EXPECT_FALSE(IsSecPurposeForPrerender(kSecPurposePrefetchHeaderValue));
  EXPECT_FALSE(IsSecPurposeForPrerender(
      kSecPurposePrefetchAnonymousClientIpHeaderValue));
  EXPECT_TRUE(
      IsSecPurposeForPrerender(kSecPurposePrefetchPrerenderHeaderValue));
  EXPECT_TRUE(
      IsSecPurposeForPrerender(kSecPurposePrefetchPrerenderPreviewHeaderValue));
  EXPECT_FALSE(IsSecPurposeForPrerender(std::nullopt));
  EXPECT_FALSE(IsSecPurposeForPrerender(""));
}

}  // namespace blink
