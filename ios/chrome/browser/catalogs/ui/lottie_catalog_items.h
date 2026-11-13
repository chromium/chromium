// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_ITEMS_H_
#define IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_ITEMS_H_

#import <Foundation/Foundation.h>

@class LottieCatalogItem;

// Returns one item per Lottie animation displayed by an animated promo, or by a
// view controller which should migrate to one, sorted by feature. Each item
// mirrors its production configuration.
NSArray<LottieCatalogItem*>* CreateLottieCatalogItems();

#endif  // IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_ITEMS_H_
