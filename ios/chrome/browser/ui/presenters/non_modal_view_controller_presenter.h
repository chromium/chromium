// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRESENTERS_NON_MODAL_VIEW_CONTROLLER_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_PRESENTERS_NON_MODAL_VIEW_CONTROLLER_PRESENTER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/presenters/contained_presenter.h"

// Presents a view controller above the base view controller in a non modal
// style. Allowing any interaction outside the presented view controller. The
// animation in is a fade in and scale down, while the animation out is just a
// fade out.
@interface NonModalViewControllerPresenter : NSObject <ContainedPresenter>
@end

#endif  // IOS_CHROME_BROWSER_UI_PRESENTERS_NON_MODAL_VIEW_CONTROLLER_PRESENTER_H_
