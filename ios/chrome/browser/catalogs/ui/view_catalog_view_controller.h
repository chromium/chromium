// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CATALOGS_UI_VIEW_CATALOG_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CATALOGS_UI_VIEW_CATALOG_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

class Browser;

// View controller for the view catalog.
@interface ViewCatalogViewController : SettingsRootTableViewController

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CATALOGS_UI_VIEW_CATALOG_VIEW_CONTROLLER_H_
