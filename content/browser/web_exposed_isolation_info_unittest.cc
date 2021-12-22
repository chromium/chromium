// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_exposed_isolation_info.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

using WebExposedIsolationInfoTest = testing::Test;

TEST_F(WebExposedIsolationInfoTest, NonIsolated) {
  WebExposedIsolationInfo info = WebExposedIsolationInfo::CreateNonIsolated();
  EXPECT_FALSE(info.is_isolated());
  EXPECT_FALSE(info.is_isolated_application());
  ASSERT_DCHECK_DEATH(info.origin());
}

TEST_F(WebExposedIsolationInfoTest, Isolated) {
  url::Origin origin =
      url::Origin::CreateFromNormalizedTuple("https", "example.com", 443);
  WebExposedIsolationInfo info =
      WebExposedIsolationInfo::CreateIsolated(origin);

  EXPECT_TRUE(info.is_isolated());
  EXPECT_FALSE(info.is_isolated_application());
  EXPECT_EQ(origin, info.origin());
}

TEST_F(WebExposedIsolationInfoTest, IsolatedApplication) {
  url::Origin origin =
      url::Origin::CreateFromNormalizedTuple("https", "example.com", 443);
  WebExposedIsolationInfo info =
      WebExposedIsolationInfo::CreateIsolatedApplication(origin);

  EXPECT_TRUE(info.is_isolated());
  EXPECT_TRUE(info.is_isolated_application());
  EXPECT_EQ(origin, info.origin());
}

TEST_F(WebExposedIsolationInfoTest, Comparisons) {
  WebExposedIsolationInfo nonA = WebExposedIsolationInfo::CreateNonIsolated();
  WebExposedIsolationInfo nonB = WebExposedIsolationInfo::CreateNonIsolated();

  // All non-isolated COII are equivalennt.
  EXPECT_EQ(nonA, nonA);
  EXPECT_EQ(nonA, nonB);
  EXPECT_EQ(nonB, nonA);
  EXPECT_FALSE(nonA < nonB);
  EXPECT_FALSE(nonB < nonA);

  url::Origin originA =
      url::Origin::CreateFromNormalizedTuple("https", "aaa.example", 443);
  url::Origin originB =
      url::Origin::CreateFromNormalizedTuple("https", "bbb.example", 443);
  WebExposedIsolationInfo isolatedA =
      WebExposedIsolationInfo::CreateIsolated(originA);
  WebExposedIsolationInfo isolatedB =
      WebExposedIsolationInfo::CreateIsolated(originB);

  // Isolated == self.
  EXPECT_EQ(isolatedA, isolatedA);
  // Non-isolated COII are < isolated COII (note that WebExposedIsolationInfo
  // only implements <, so we can't use EXPECT_GT here and below).
  EXPECT_LT(nonA, isolatedA);
  EXPECT_FALSE(isolatedA < nonA);
  // Origin comparison for isolated < isolated
  EXPECT_LT(isolatedA, isolatedB);
  EXPECT_FALSE(isolatedB < isolatedA);

  WebExposedIsolationInfo appA =
      WebExposedIsolationInfo::CreateIsolatedApplication(originA);
  WebExposedIsolationInfo appB =
      WebExposedIsolationInfo::CreateIsolatedApplication(originB);

  // Isolated app == self.
  EXPECT_EQ(appA, appA);
  // Non-isolated COII are < isolated app COII.
  EXPECT_LT(nonA, appA);
  EXPECT_FALSE(appA < nonA);
  // Non-isolated COII are < isolated app COII.
  EXPECT_LT(isolatedA, appA);
  EXPECT_FALSE(appA < isolatedA);
  // Origin comparison for isolated app < isolated app
  EXPECT_LT(appA, appB);
  EXPECT_FALSE(appB < appA);
}

TEST_F(WebExposedIsolationInfoTest, AreCompatibleFunctions) {
  url::Origin originA =
      url::Origin::CreateFromNormalizedTuple("https", "aaa.example", 443);
  url::Origin originB =
      url::Origin::CreateFromNormalizedTuple("https", "bbb.example", 443);
  WebExposedIsolationInfo nonIsolated =
      WebExposedIsolationInfo::CreateNonIsolated();
  WebExposedIsolationInfo isolatedA =
      WebExposedIsolationInfo::CreateIsolated(originA);
  WebExposedIsolationInfo isolatedB =
      WebExposedIsolationInfo::CreateIsolated(originB);
  WebExposedIsolationInfo appA =
      WebExposedIsolationInfo::CreateIsolatedApplication(originA);
  WebExposedIsolationInfo appB =
      WebExposedIsolationInfo::CreateIsolatedApplication(originB);

  absl::optional<WebExposedIsolationInfo> optionalEmpty = absl::nullopt;
  EXPECT_TRUE(
      WebExposedIsolationInfo::AreCompatible(optionalEmpty, nonIsolated));
  EXPECT_TRUE(WebExposedIsolationInfo::AreCompatible(optionalEmpty, isolatedA));
  EXPECT_TRUE(WebExposedIsolationInfo::AreCompatible(optionalEmpty, appA));
  EXPECT_TRUE(
      WebExposedIsolationInfo::AreCompatible(nonIsolated, optionalEmpty));
  EXPECT_TRUE(WebExposedIsolationInfo::AreCompatible(isolatedA, optionalEmpty));
  EXPECT_TRUE(WebExposedIsolationInfo::AreCompatible(appA, optionalEmpty));

  absl::optional<WebExposedIsolationInfo> optionalNonIsolated =
      WebExposedIsolationInfo::CreateNonIsolated();
  EXPECT_TRUE(
      WebExposedIsolationInfo::AreCompatible(optionalNonIsolated, nonIsolated));
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(optionalNonIsolated, isolatedA));
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(optionalNonIsolated, appA));
  EXPECT_TRUE(
      WebExposedIsolationInfo::AreCompatible(nonIsolated, optionalNonIsolated));
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(isolatedA, optionalNonIsolated));
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(appA, optionalNonIsolated));

  absl::optional<WebExposedIsolationInfo> optionalIsolatedA =
      WebExposedIsolationInfo::CreateIsolated(originA);
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(optionalIsolatedA, nonIsolated));
  EXPECT_TRUE(
      WebExposedIsolationInfo::AreCompatible(optionalIsolatedA, isolatedA));
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(optionalIsolatedA, isolatedB));
  EXPECT_FALSE(WebExposedIsolationInfo::AreCompatible(optionalIsolatedA, appA));
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(nonIsolated, optionalIsolatedA));
  EXPECT_TRUE(
      WebExposedIsolationInfo::AreCompatible(isolatedA, optionalIsolatedA));
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(isolatedB, optionalIsolatedA));
  EXPECT_FALSE(WebExposedIsolationInfo::AreCompatible(appA, optionalIsolatedA));

  absl::optional<WebExposedIsolationInfo> optionalAppA =
      WebExposedIsolationInfo::CreateIsolatedApplication(originA);
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(optionalAppA, nonIsolated));
  EXPECT_FALSE(WebExposedIsolationInfo::AreCompatible(optionalAppA, isolatedA));
  EXPECT_TRUE(WebExposedIsolationInfo::AreCompatible(optionalAppA, appA));
  EXPECT_FALSE(WebExposedIsolationInfo::AreCompatible(optionalAppA, appB));
  EXPECT_FALSE(
      WebExposedIsolationInfo::AreCompatible(nonIsolated, optionalAppA));
  EXPECT_FALSE(WebExposedIsolationInfo::AreCompatible(isolatedA, optionalAppA));
  EXPECT_TRUE(WebExposedIsolationInfo::AreCompatible(appA, optionalAppA));
  EXPECT_FALSE(WebExposedIsolationInfo::AreCompatible(appB, optionalAppA));
}

}  // namespace content
