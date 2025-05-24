// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_MOCK_FAVICON_LOADER_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_MOCK_FAVICON_LOADER_H_

#include "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#include "testing/gmock/include/gmock/gmock.h"

// A test double for FaviconLoader that allow mocking methods.
//
// If possible prefer to use TestFaviconLoader if you only need a
// test double that successfully returns a valid favicon for all
// invocations.
class MockFaviconLoader : public TestFaviconLoader {
 public:
  MockFaviconLoader();
  ~MockFaviconLoader() override;

  // Mockable methods.
  MOCK_METHOD(void,
              FaviconForPageUrl,
              (const GURL& page_url,
               float size_in_points,
               float min_size_in_points,
               bool fallback_to_google_server,
               FaviconAttributesCompletionBlock favicon_block_handler),
              (override));
  MOCK_METHOD(void,
              FaviconForPageUrlOrHost,
              (const GURL& page_url,
               float size_in_points,
               FaviconAttributesCompletionBlock favicon_block_handler),
              (override));
  MOCK_METHOD(void,
              FaviconForIconUrl,
              (const GURL& icon_url,
               float size_in_points,
               float min_size_in_points,
               FaviconAttributesCompletionBlock favicon_block_handler),
              (override));
  MOCK_METHOD(void, CancellAllRequests, (), (override));
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_MOCK_FAVICON_LOADER_H_
