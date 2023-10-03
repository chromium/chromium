// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"

#import <memory>
#import <string>

#import "base/files/file_path.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/gtest_util.h"
#import "base/time/default_clock.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using OfflineURLUtilsTest = PlatformTest;

// Checks the distilled URL for the page with an onlineURL is
// chrome://offline/?entryURL=...
TEST_F(OfflineURLUtilsTest, OfflineURLForURL) {
  GURL entry_url = GURL("http://foo.bar");
  GURL distilled_url = reading_list::OfflineURLForURL(entry_url);
  EXPECT_EQ("chrome://offline/?"
            "entryURL=http%3A%2F%2Ffoo.bar%2F",
            distilled_url.spec());
}

// Checks the parsing of offline URL chrome://offline/MD5/page.html.
// As entryURL is absent, it should be invalid.
TEST_F(OfflineURLUtilsTest, ParseOfflineURLTest) {
  GURL distilled_url("chrome://offline/MD5/page.html");
  GURL entry_url = reading_list::EntryURLForOfflineURL(distilled_url);
  EXPECT_TRUE(entry_url.is_empty());
}

// Checks the parsing of offline URL
// chrome://offline/MD5/page.html?entryURL=encorded%20URL
// As entryURL is present, it should be returned correctly.
TEST_F(OfflineURLUtilsTest, ParseOfflineURLWithEntryURLTest) {
  GURL offline_url(
      "chrome://offline/MD5/page.html?entryURL=http%3A%2F%2Ffoo.bar%2F");
  GURL entry_url = reading_list::EntryURLForOfflineURL(offline_url);
  EXPECT_EQ("http://foo.bar/", entry_url.spec());
}

// Checks the parsing of offline URL
// chrome://offline/MD5/page.html
// As entryURL is absent, it should return the offline URL.
TEST_F(OfflineURLUtilsTest, ParseOfflineURLWithVirtualURLTest) {
  GURL offline_url("chrome://offline/MD5/page.html");
  GURL entry_url = reading_list::EntryURLForOfflineURL(offline_url);
  EXPECT_TRUE(entry_url.is_empty());
}

// Checks that the offline URLs are correctly detected by `IsOfflineURL`.
TEST_F(OfflineURLUtilsTest, IsOfflineURL) {
  EXPECT_FALSE(reading_list::IsOfflineURL(GURL()));
  EXPECT_FALSE(reading_list::IsOfflineURL(GURL("chrome://")));
  EXPECT_FALSE(reading_list::IsOfflineURL(GURL("chrome://offline-foobar")));
  EXPECT_FALSE(reading_list::IsOfflineURL(GURL("http://offline/")));
  EXPECT_FALSE(reading_list::IsOfflineURL(GURL("http://chrome://offline/")));
  EXPECT_TRUE(reading_list::IsOfflineURL(GURL("chrome://offline")));
  EXPECT_TRUE(reading_list::IsOfflineURL(GURL("chrome://offline/")));
  EXPECT_TRUE(reading_list::IsOfflineURL(GURL("chrome://offline/foobar")));
  EXPECT_TRUE(
      reading_list::IsOfflineURL(GURL("chrome://offline/foobar?foo=bar")));
  EXPECT_TRUE(reading_list::IsOfflineURL(
      GURL("chrome://offline/foobar?entryURL=http%3A%2F%2Ffoo.bar%2F")));
  EXPECT_TRUE(reading_list::IsOfflineURL(
      GURL("chrome://offline/foobar?reload=http%3A%2F%2Ffoo.bar%2F")));
}

// Checks that the offline URLs are correctly detected by `IsOfflineEntryURL`.
TEST_F(OfflineURLUtilsTest, IsOfflineEntryURL) {
  EXPECT_FALSE(reading_list::IsOfflineEntryURL(GURL()));
  EXPECT_FALSE(reading_list::IsOfflineEntryURL(GURL("chrome://")));
  EXPECT_FALSE(
      reading_list::IsOfflineEntryURL(GURL("chrome://offline-foobar")));
  EXPECT_FALSE(reading_list::IsOfflineEntryURL(GURL("http://offline/")));
  EXPECT_FALSE(
      reading_list::IsOfflineEntryURL(GURL("http://chrome://offline/")));
  EXPECT_FALSE(reading_list::IsOfflineEntryURL(GURL("chrome://offline")));
  EXPECT_FALSE(reading_list::IsOfflineEntryURL(GURL("chrome://offline/")));
  EXPECT_FALSE(
      reading_list::IsOfflineEntryURL(GURL("chrome://offline/foobar")));
  EXPECT_FALSE(
      reading_list::IsOfflineEntryURL(GURL("chrome://offline/foobar?foo=bar")));
  EXPECT_TRUE(reading_list::IsOfflineEntryURL(
      GURL("chrome://offline/foobar?entryURL=http%3A%2F%2Ffoo.bar%2F")));
  EXPECT_FALSE(reading_list::IsOfflineEntryURL(
      GURL("chrome://offline/foobar?reload=http%3A%2F%2Ffoo.bar%2F")));
}

// Checks that the offline URLs are correctly detected by `IsOfflineReloadURL`.
TEST_F(OfflineURLUtilsTest, IsOfflineReloadURL) {
  EXPECT_FALSE(reading_list::IsOfflineReloadURL(GURL()));
  EXPECT_FALSE(reading_list::IsOfflineReloadURL(GURL("chrome://")));
  EXPECT_FALSE(
      reading_list::IsOfflineReloadURL(GURL("chrome://offline-foobar")));
  EXPECT_FALSE(reading_list::IsOfflineReloadURL(GURL("http://offline/")));
  EXPECT_FALSE(
      reading_list::IsOfflineReloadURL(GURL("http://chrome://offline/")));
  EXPECT_FALSE(reading_list::IsOfflineReloadURL(GURL("chrome://offline")));
  EXPECT_FALSE(reading_list::IsOfflineReloadURL(GURL("chrome://offline/")));
  EXPECT_FALSE(
      reading_list::IsOfflineReloadURL(GURL("chrome://offline/foobar")));
  EXPECT_FALSE(reading_list::IsOfflineReloadURL(
      GURL("chrome://offline/foobar?foo=bar")));
  EXPECT_FALSE(reading_list::IsOfflineReloadURL(
      GURL("chrome://offline/foobar?entryURL=http%3A%2F%2Ffoo.bar%2F")));
  EXPECT_TRUE(reading_list::IsOfflineReloadURL(
      GURL("chrome://offline/foobar?reload=http%3A%2F%2Ffoo.bar%2F")));
}

// Checks the offline URL to reload URL is
// chrome://offline?reload=URL
TEST_F(OfflineURLUtilsTest, OfflineReloadURLForURLTest) {
  GURL reload_url = GURL("http://foo.bar");
  GURL offline_url = reading_list::OfflineReloadURLForURL(reload_url);
  EXPECT_EQ("chrome://offline/?reload=http%3A%2F%2Ffoo.bar%2F",
            offline_url.spec());
}

// Extracts the reload URL from chrome://offline?reload=URL
TEST_F(OfflineURLUtilsTest, ReloadURLForOfflineURLTest) {
  GURL offline_url = GURL("chrome://offline?reload=http%3A%2F%2Ffoo.bar%2F");
  GURL reload_url = reading_list::ReloadURLForOfflineURL(offline_url);
  EXPECT_EQ("http://foo.bar/", reload_url.spec());
}
