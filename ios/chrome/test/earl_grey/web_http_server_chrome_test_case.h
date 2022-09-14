// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_WEB_HTTP_SERVER_CHROME_TEST_CASE_H_
#define IOS_CHROME_TEST_EARL_GREY_WEB_HTTP_SERVER_CHROME_TEST_CASE_H_

#import "ios/chrome/test/earl_grey/chrome_test_case.h"

// Base class for Chrome Earl Grey tests which need a web::test::HttpServer.
// NOTE: This class exists for compatibility with old test classes only.
//       New tests should not use this class, but instead inherit from
//       ChromeTestCase and use `self.testServer` directly.
@interface WebHttpServerChromeTestCase : ChromeTestCase
@end

#endif  // IOS_CHROME_TEST_EARL_GREY_WEB_HTTP_SERVER_CHROME_TEST_CASE_H_
