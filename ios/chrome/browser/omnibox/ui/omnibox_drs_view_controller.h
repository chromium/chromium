// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_DRS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_DRS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_presenter.h"

@interface OmniboxDRSViewController
    : UIViewController <OmniboxPopupPresenterDelegate>

@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    proxiedPresenterDelegate;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_DRS_VIEW_CONTROLLER_H_
