// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

TestFaviconLoader::TestFaviconLoader() {
  default_attributes_ = [FaviconAttributes
      attributesWithImage:[UIImage imageNamed:@"default_world_favicon"]];
}

TestFaviconLoader::~TestFaviconLoader() = default;

void TestFaviconLoader::FaviconForPageUrl(
    const GURL& page_url,
    float size_in_points,
    float min_size_in_points,
    bool fallback_to_google_server,
    FaviconAttributesCompletionBlock favicon_block_handler) {
  favicon_block_handler(default_attributes_, /*cached*/ true);
}

void TestFaviconLoader::FaviconForPageUrlOrHost(
    const GURL& page_url,
    float size_in_points,
    FaviconAttributesCompletionBlock favicon_block_handler) {
  favicon_block_handler(default_attributes_, /*cached*/ true);
}

void TestFaviconLoader::FaviconForIconUrl(
    const GURL& icon_url,
    float size_in_points,
    float min_size_in_points,
    FaviconAttributesCompletionBlock favicon_block_handler) {
  favicon_block_handler(default_attributes_, /*cached*/ true);
}

void TestFaviconLoader::CancellAllRequests() {
  // Nothing to do, all requests are resolved synchronously.
}

base::WeakPtr<FaviconLoader> TestFaviconLoader::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
