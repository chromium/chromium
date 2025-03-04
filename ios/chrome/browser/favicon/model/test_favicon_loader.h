// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_TEST_FAVICON_LOADER_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_TEST_FAVICON_LOADER_H_

#include "ios/chrome/browser/favicon/model/favicon_loader.h"

// A test implementation of FaviconLoader that always return the same
// image for all requests.
class TestFaviconLoader : public FaviconLoader {
 public:
  TestFaviconLoader();

  ~TestFaviconLoader() override;

  // FaviconLoader implementation.
  void FaviconForPageUrl(
      const GURL& page_url,
      float size_in_points,
      float min_size_in_points,
      bool fallback_to_google_server,
      FaviconAttributesCompletionBlock favicon_block_handler) override;
  void FaviconForPageUrlOrHost(
      const GURL& page_url,
      float size_in_points,
      FaviconAttributesCompletionBlock favicon_block_handler) override;
  void FaviconForIconUrl(
      const GURL& icon_url,
      float size_in_points,
      float min_size_in_points,
      FaviconAttributesCompletionBlock favicon_block_handler) override;
  void CancellAllRequests() override;
  base::WeakPtr<FaviconLoader> AsWeakPtr() override;

 private:
  __strong FaviconAttributes* default_attributes_;

  base::WeakPtrFactory<TestFaviconLoader> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_TEST_FAVICON_LOADER_H_
