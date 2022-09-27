// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {
namespace {

const char kTestScheme[] = "test-scheme";
const char kTestScheme2[] = "test-scheme-2";

class SchemeRegistryTest : public testing::Test {
  void TearDown() override {
#if DCHECK_IS_ON()
    WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
    SchemeRegistry::RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
        kTestScheme);
  }
};

TEST_F(SchemeRegistryTest, NoCSPBypass) {
  EXPECT_FALSE(
      SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(kTestScheme));
  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
      kTestScheme, SchemeRegistry::kPolicyAreaImage));
}

TEST_F(SchemeRegistryTest, FullCSPBypass) {
#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy(
      kTestScheme);
  EXPECT_TRUE(
      SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(kTestScheme));
  EXPECT_TRUE(SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
      kTestScheme, SchemeRegistry::kPolicyAreaImage));
  EXPECT_FALSE(
      SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(kTestScheme2));
  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
      kTestScheme2, SchemeRegistry::kPolicyAreaImage));
}

TEST_F(SchemeRegistryTest, PartialCSPBypass) {
#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RegisterURLSchemeAsBypassingContentSecurityPolicy(
      kTestScheme, SchemeRegistry::kPolicyAreaImage);
  EXPECT_FALSE(
      SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(kTestScheme));
  EXPECT_TRUE(SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
      kTestScheme, SchemeRegistry::kPolicyAreaImage));
  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
      kTestScheme, SchemeRegistry::kPolicyAreaStyle));
  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
      kTestScheme2, SchemeRegistry::kPolicyAreaImage));
}

TEST_F(SchemeRegistryTest, BypassSecureContextCheck) {
  const char* scheme1 = "http";
  const char* scheme2 = "https";
  const char* scheme3 = "random-scheme";

  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassSecureContextCheck(scheme1));
  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassSecureContextCheck(scheme2));
  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassSecureContextCheck(scheme3));

#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RegisterURLSchemeBypassingSecureContextCheck("random-scheme");

  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassSecureContextCheck(scheme1));
  EXPECT_FALSE(SchemeRegistry::SchemeShouldBypassSecureContextCheck(scheme2));
  EXPECT_TRUE(SchemeRegistry::SchemeShouldBypassSecureContextCheck(scheme3));
}

TEST_F(SchemeRegistryTest, WebUIScheme) {
  const char* kChromeUIScheme = "chrome";
  EXPECT_FALSE(SchemeRegistry::IsWebUIScheme(kTestScheme));
  EXPECT_FALSE(SchemeRegistry::IsWebUIScheme(kChromeUIScheme));

#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RegisterURLSchemeAsWebUI(kTestScheme);

  EXPECT_TRUE(SchemeRegistry::IsWebUIScheme(kTestScheme));
  EXPECT_FALSE(SchemeRegistry::IsWebUIScheme(kChromeUIScheme));

  SchemeRegistry::RegisterURLSchemeAsWebUI(kChromeUIScheme);

  EXPECT_TRUE(SchemeRegistry::IsWebUIScheme(kTestScheme));
  EXPECT_TRUE(SchemeRegistry::IsWebUIScheme(kChromeUIScheme));

  SchemeRegistry::RemoveURLSchemeAsWebUI(kTestScheme);

  EXPECT_FALSE(SchemeRegistry::IsWebUIScheme(kTestScheme));
  EXPECT_TRUE(SchemeRegistry::IsWebUIScheme(kChromeUIScheme));

  SchemeRegistry::RemoveURLSchemeAsWebUI(kChromeUIScheme);

  EXPECT_FALSE(SchemeRegistry::IsWebUIScheme(kTestScheme));
  EXPECT_FALSE(SchemeRegistry::IsWebUIScheme(kChromeUIScheme));
}

TEST_F(SchemeRegistryTest, ExtensionScheme) {
  const char* kExtensionScheme = "chrome-extension";
  EXPECT_FALSE(CommonSchemeRegistry::IsExtensionScheme(kTestScheme));
  EXPECT_FALSE(CommonSchemeRegistry::IsExtensionScheme(kExtensionScheme));

#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  CommonSchemeRegistry::RegisterURLSchemeAsExtension(kExtensionScheme);

  EXPECT_FALSE(CommonSchemeRegistry::IsExtensionScheme(kTestScheme));
  EXPECT_TRUE(CommonSchemeRegistry::IsExtensionScheme(kExtensionScheme));

  CommonSchemeRegistry::RegisterURLSchemeAsExtension(kTestScheme);

  EXPECT_TRUE(CommonSchemeRegistry::IsExtensionScheme(kTestScheme));
  EXPECT_TRUE(CommonSchemeRegistry::IsExtensionScheme(kExtensionScheme));

  CommonSchemeRegistry::RemoveURLSchemeAsExtensionForTest(kExtensionScheme);

  EXPECT_TRUE(CommonSchemeRegistry::IsExtensionScheme(kTestScheme));
  EXPECT_FALSE(CommonSchemeRegistry::IsExtensionScheme(kExtensionScheme));

  CommonSchemeRegistry::RemoveURLSchemeAsExtensionForTest(kTestScheme);

  EXPECT_FALSE(CommonSchemeRegistry::IsExtensionScheme(kTestScheme));
  EXPECT_FALSE(CommonSchemeRegistry::IsExtensionScheme(kExtensionScheme));
}

TEST_F(SchemeRegistryTest, CodeCacheWithHashing) {
  const char* kChromeUIScheme = "chrome";
  EXPECT_FALSE(SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kTestScheme));
  EXPECT_FALSE(
      SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kChromeUIScheme));

  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(kTestScheme);

  EXPECT_TRUE(SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kTestScheme));
  EXPECT_FALSE(
      SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kChromeUIScheme));

  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(kChromeUIScheme);

  EXPECT_TRUE(SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kTestScheme));
  EXPECT_TRUE(
      SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kChromeUIScheme));

  SchemeRegistry::RemoveURLSchemeAsCodeCacheWithHashing(kTestScheme);

  EXPECT_FALSE(SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kTestScheme));
  EXPECT_TRUE(
      SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kChromeUIScheme));

  SchemeRegistry::RemoveURLSchemeAsCodeCacheWithHashing(kChromeUIScheme);

  EXPECT_FALSE(SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kTestScheme));
  EXPECT_FALSE(
      SchemeRegistry::SchemeSupportsCodeCacheWithHashing(kChromeUIScheme));
}

}  // namespace
}  // namespace blink
