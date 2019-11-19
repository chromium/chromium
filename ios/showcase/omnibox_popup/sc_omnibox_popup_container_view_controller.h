// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_CONTAINER_VIEW_CONTROLLER_H_

#include <UIKit/UIKit.h>

@class OmniboxPopupBaseViewController;

// In the main app, the |OmniboxPopupViewController| view is contained inside
// another view (see |OmniboxPopupPresenter|). This class mimics that for
// Showcase.
@interface SCOmniboxPopupContainerViewController : UIViewController

@property(nonatomic, strong)
    OmniboxPopupBaseViewController* popupViewController;

- (instancetype)initWithPopupViewController:
    (OmniboxPopupBaseViewController*)popupViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_SHOWCASE_OMNIBOX_POPUP_SC_OMNIBOX_POPUP_CONTAINER_VIEW_CONTROLLER_H_
