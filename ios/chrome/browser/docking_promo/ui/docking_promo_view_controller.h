// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/docking_promo/ui/animated_promo_view_controller.h"

// Container view controller for the Docking Promo.
@interface DockingPromoViewController : AnimatedPromoViewController

// Creates a new instance of the view controller. If `remindMeLater` is YES, the
// view controller will include a "Remind Me Later" button. Otherwise, the view
// controller will contain a "No Thanks" button.
- (instancetype)initWithRemindMeLater:(BOOL)remindMeLater
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_VIEW_CONTROLLER_H_
