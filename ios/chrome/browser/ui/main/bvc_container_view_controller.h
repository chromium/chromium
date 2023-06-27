// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_BVC_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_BVC_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// A UIViewController instance designed to contain an instance of
// BrowserViewController ("BVC") as a child. Since the BVC itself often
// implements a great deal of custom logic around handling view controller
// presentation and other features, this containing view controller handles
// forwarding calls to the BVC instance where needed.
@interface BVCContainerViewController : UIViewController

// The BVC instance being contained. If this is set, the current BVC (if any)
// will be removed as a child view controller, and the new `currentBVC` will
// be added as a child and have its view resized to this object's view's bounds.
@property(nonatomic, weak) UIViewController* currentBVC;

// Fallback presenter VC to use when `currentBVC` is nil. Owner of this VC
// should set this property, which is used by
// `presentViewController:animated:completion:` and
// `dismissViewControllerAnimated:completion:`.
@property(nonatomic, weak) UIViewController* fallbackPresenterViewController;

// YES if the currentBVC is in incognito mode. Is used to set proper background
// color.
@property(nonatomic, assign) BOOL incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_BVC_CONTAINER_VIEW_CONTROLLER_H_
