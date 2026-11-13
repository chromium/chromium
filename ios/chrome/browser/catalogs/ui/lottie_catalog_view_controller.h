// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

// Catalog previewing the Lottie animations displayed by animated promos, or by
// view controllers which should migrate to one. The animation is picked from a
// menu grouped by feature, and presented in an animated promo mirroring its
// production configuration, optionally wrapped in a navigation controller.
@interface LottieCatalogViewController : SettingsRootTableViewController

// Initializes the catalog with an inset grouped style.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_VIEW_CONTROLLER_H_
