// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/offline_url_utils.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/time/default_clock.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using OfflineURLUtilsTest = PlatformTest;

// Checks the distilled URL for the page with an onlineURL is
// chrome://offline/MD5/page.html?entryURL=...&virtualURL=...
TEST_F(OfflineURLUtilsTest, OfflineURLForPathWithEntryURLAndVirtualURLTest) {
  base::FilePath page_path("MD5/page.html");
  GURL entry_url = GURL("http://foo.bar");
  GURL virtual_url = GURL("http://foo.bar/virtual");
  GURL distilled_url =
      reading_list::OfflineURLForPath(page_path, entry_url, virtual_url);
  EXPECT_EQ("chrome://offline/MD5/page.html?"
            "entryURL=http%3A%2F%2Ffoo.bar%2F&"
            "virtualURL=http%3A%2F%2Ffoo.bar%2Fvirtual",
            distilled_url.spec());
}

// Checks the parsing of offline URL chrome://offline/MD5/page.html.
// As entryURL and virtualURL are absent, they should be invalid.
TEST_F(OfflineURLUtilsTest, ParseOfflineURLTest) {
  GURL distilled_url("chrome://offline/MD5/page.html");
  GURL entry_url = reading_list::EntryURLForOfflineURL(distilled_url);
  EXPECT_TRUE(entry_url.is_empty());
  GURL virtual_url = reading_list::VirtualURLForOfflineURL(distilled_url);
  EXPECT_TRUE(virtual_url.is_empty());
}

// Checks the parsing of offline URL
// chrome://offline/MD5/page.html?entryURL=encorded%20URL
// As entryURL is present, it should be returned correctly.
// As virtualURL is absent, it should return GURL::EmptyGURL().
TEST_F(OfflineURLUtilsTest, ParseOfflineURLWithEntryURLTest) {
  GURL offline_url(
      "chrome://offline/MD5/page.html?entryURL=http%3A%2F%2Ffoo.bar%2F");
  GURL entry_url = reading_list::EntryURLForOfflineURL(offline_url);
  EXPECT_EQ("http://foo.bar/", entry_url.spec());
  GURL virtual_url = reading_list::VirtualURLForOfflineURL(offline_url);
  EXPECT_TRUE(virtual_url.is_empty());
}

// Checks the parsing of offline URL
// chrome://offline/MD5/page.html?virtualURL=encorded%20URL
// As entryURL is absent, it should return the offline URL.
// As virtualURL is present, it should be returned correctly.
TEST_F(OfflineURLUtilsTest, ParseOfflineURLWithVirtualURLTest) {
  GURL offline_url(
      "chrome://offline/MD5/page.html?virtualURL=http%3A%2F%2Ffoo.bar%2F");
  GURL entry_url = reading_list::EntryURLForOfflineURL(offline_url);
  EXPECT_TRUE(entry_url.is_empty());
  GURL virtual_url = reading_list::VirtualURLForOfflineURL(offline_url);
  EXPECT_EQ("http://foo.bar/", virtual_url.spec());
}

// Checks the parsing of offline URL
// chrome://offline/MD5/page.html?entryURL=...&virtualURL=...
// As entryURL is present, it should be returned correctly.
// As virtualURL is present, it should be returned correctly.
TEST_F(OfflineURLUtilsTest, ParseOfflineURLWithVirtualAndEntryURLTest) {
  GURL offline_url(
      "chrome://offline/MD5/"
      "page.html?virtualURL=http%3A%2F%2Ffoo.bar%2Fvirtual&entryURL=http%3A%2F%"
      "2Ffoo.bar%2Fentry");
  GURL entry_url = reading_list::EntryURLForOfflineURL(offline_url);
  EXPECT_EQ("http://foo.bar/entry", entry_url.spec());
  GURL virtual_url = reading_list::VirtualURLForOfflineURL(offline_url);
  EXPECT_EQ("http://foo.bar/virtual", virtual_url.spec());
}

// Checks the file path for chrome://offline/MD5/page.html is
// file://profile_path/Offline/MD5/page.html.
// Checks the resource root for chrome://offline/MD5/page.html is
// file://profile_path/Offline/MD5
TEST_F(OfflineURLUtilsTest, FileURLForDistilledURLTest) {
  base::FilePath offline_path("/profile_path/Offline");
  GURL file_url =
      reading_list::FileURLForDistilledURL(GURL(), offline_path, nullptr);
  EXPECT_FALSE(file_url.is_valid());

  GURL distilled_url("chrome://offline/MD5/page.html");
  file_url = reading_list::FileURLForDistilledURL(distilled_url, offline_path,
                                                  nullptr);
  EXPECT_TRUE(file_url.is_valid());
  EXPECT_TRUE(file_url.SchemeIsFile());
  EXPECT_EQ("/profile_path/Offline/MD5/page.html", file_url.path());

  GURL resource_url;
  file_url = reading_list::FileURLForDistilledURL(distilled_url, offline_path,
                                                  &resource_url);
  EXPECT_TRUE(resource_url.is_valid());
  EXPECT_TRUE(resource_url.SchemeIsFile());
  EXPECT_EQ("/profile_path/Offline/MD5/", resource_url.path());
}

// Checks that the offline URLs are correctly detected by |IsOfflineURL|.
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

// Checks that the offline URLs are correctly detected by |IsOfflineEntryURL|.
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

// Checks that the offline URLs are correctly detected by |IsOfflineReloadURL|.
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
