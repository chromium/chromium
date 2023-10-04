// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/model/translate_service_ios.h"

#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using TranslateServiceIOSTest = PlatformTest;

TEST_F(TranslateServiceIOSTest, CheckTranslatableURL) {
  GURL empty_url = GURL(std::string());
  EXPECT_FALSE(TranslateServiceIOS::IsTranslatableURL(empty_url));

  GURL chrome_url = GURL(kChromeUIFlagsURL);
  EXPECT_FALSE(TranslateServiceIOS::IsTranslatableURL(chrome_url));

  GURL right_url = GURL("http://www.tamurayukari.com/");
  EXPECT_TRUE(TranslateServiceIOS::IsTranslatableURL(right_url));
}
