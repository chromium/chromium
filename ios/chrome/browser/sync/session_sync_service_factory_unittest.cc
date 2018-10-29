// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/session_sync_service_factory.h"

#include "ios/chrome/browser/chrome_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kValidUrl[] = "http://www.example.com";
const char kInvalidUrl[] = "invalid.url";

TEST(SessionSyncServiceFactoryTest, ShouldSyncURL) {
  EXPECT_TRUE(
      SessionSyncServiceFactory::ShouldSyncURLForTesting(GURL(kValidUrl)));
  EXPECT_TRUE(SessionSyncServiceFactory::ShouldSyncURLForTesting(
      GURL("other://anything")));
  EXPECT_TRUE(SessionSyncServiceFactory::ShouldSyncURLForTesting(
      GURL("chrome-other://anything")));

  EXPECT_FALSE(
      SessionSyncServiceFactory::ShouldSyncURLForTesting(GURL(kInvalidUrl)));
  EXPECT_FALSE(SessionSyncServiceFactory::ShouldSyncURLForTesting(
      GURL("file://anything")));
  EXPECT_FALSE(SessionSyncServiceFactory::ShouldSyncURLForTesting(
      GURL("chrome://anything")));

  EXPECT_TRUE(SessionSyncServiceFactory::ShouldSyncURLForTesting(
      GURL(kChromeUIHistoryURL)));
}

}  // namespace
