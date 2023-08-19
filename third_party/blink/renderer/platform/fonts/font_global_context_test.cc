// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_global_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::test::CreateTestFont;

namespace blink {

class FontGlobalContextTest : public FontTestBase {};

TEST_F(FontGlobalContextTest, TypeFaceDigestCacheSameEntry) {
  // Put IdentifiableToken of Ahem in cache
  IdentifiableToken digest_1 =
      FontGlobalContext::Get().GetOrComputeTypefaceDigest(
          CreateTestFont(AtomicString("Ahem"),
                         test::PlatformTestDataPath("Ahem.woff"), 16)
              .PrimaryFont()
              ->PlatformData());

  // Get IdentifiableToken of Ahem in cache
  IdentifiableToken digest_2 =
      FontGlobalContext::Get().GetOrComputeTypefaceDigest(
          CreateTestFont(AtomicString("Ahem"),
                         test::PlatformTestDataPath("Ahem.woff"), 16)
              .PrimaryFont()
              ->PlatformData());
  EXPECT_EQ(digest_1, digest_2);
}

TEST_F(FontGlobalContextTest, TypeFaceDigestCacheDifferentEntry) {
  // Put IdentifiableToken of Ahem in cache
  IdentifiableToken digest_ahem =
      FontGlobalContext::Get().GetOrComputeTypefaceDigest(
          CreateTestFont(AtomicString("Ahem"),
                         test::PlatformTestDataPath("Ahem.woff"), 16)
              .PrimaryFont()
              ->PlatformData());

  // Put IdentifiableToken of AhemSpaceLigature in cache
  IdentifiableToken digest_ahem_space_ligature =
      FontGlobalContext::Get().GetOrComputeTypefaceDigest(
          CreateTestFont(AtomicString("AhemSpaceLigature"),
                         test::PlatformTestDataPath("AhemSpaceLigature.woff"),
                         16)
              .PrimaryFont()
              ->PlatformData());
  EXPECT_NE(digest_ahem, digest_ahem_space_ligature);
}

TEST_F(FontGlobalContextTest, PostScriptNameDigestCacheSameEntry) {
  // Put IdentifiableToken of Ahem in cache
  IdentifiableToken digest_1 =
      FontGlobalContext::Get().GetOrComputePostScriptNameDigest(
          CreateTestFont(AtomicString("Ahem"),
                         test::PlatformTestDataPath("Ahem.woff"), 16)
              .PrimaryFont()
              ->PlatformData());

  // Get IdentifiableToken of Ahem in cache
  IdentifiableToken digest_2 =
      FontGlobalContext::Get().GetOrComputePostScriptNameDigest(
          CreateTestFont(AtomicString("Ahem"),
                         test::PlatformTestDataPath("Ahem.woff"), 16)
              .PrimaryFont()
              ->PlatformData());
  EXPECT_EQ(digest_1, digest_2);
}

TEST_F(FontGlobalContextTest, PostScriptNameDigestCacheDifferentEntry) {
  // Put IdentifiableToken of Ahem in cache
  IdentifiableToken digest_ahem =
      FontGlobalContext::Get().GetOrComputePostScriptNameDigest(
          CreateTestFont(AtomicString("Ahem"),
                         test::PlatformTestDataPath("Ahem.woff"), 16)
              .PrimaryFont()
              ->PlatformData());

  // Put IdentifiableToken of AhemSpaceLigature in cache
  IdentifiableToken digest_ahem_space_ligature =
      FontGlobalContext::Get().GetOrComputePostScriptNameDigest(
          CreateTestFont(AtomicString("AhemSpaceLigature"),
                         test::PlatformTestDataPath("AhemSpaceLigature.woff"),
                         16)
              .PrimaryFont()
              ->PlatformData());
  EXPECT_NE(digest_ahem, digest_ahem_space_ligature);
}

}  // namespace blink
