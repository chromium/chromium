// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_MOCK_FAVICON_LOADER_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_MOCK_FAVICON_LOADER_H_

#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "url/gurl.h"

class MockFaviconLoader : public FaviconLoader {
 public:
  MockFaviconLoader();
  ~MockFaviconLoader() override;

  MOCK_METHOD(
      void,
      FaviconForPageUrl,
      (const GURL&, float, float, bool, FaviconAttributesCompletionBlock),
      (override));
  MOCK_METHOD(void,
              FaviconForPageUrlOrHost,
              (const GURL&, float, FaviconAttributesCompletionBlock),
              (override));
  MOCK_METHOD(void,
              FaviconForIconUrl,
              (const GURL&, float, float, FaviconAttributesCompletionBlock),
              (override));
  MOCK_METHOD(void, CancellAllRequests, (), (override));
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_MOCK_FAVICON_LOADER_H_
