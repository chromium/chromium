// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_UI_CATALOG_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_UI_CATALOG_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller_protocol.h"

// View controller displaying a catalog of micro UI experiences and debug pages
// created during AI prototyping.
@interface AIPrototypingUICatalogViewController : UITableViewController

@end

// Navigation controller wrapping the catalog view controller and conforming to
// the AIPrototypingViewControllerProtocol.
@interface AIPrototypingUICatalogNavigationController
    : UINavigationController <AIPrototypingViewControllerProtocol>

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_UI_CATALOG_VIEW_CONTROLLER_H_
