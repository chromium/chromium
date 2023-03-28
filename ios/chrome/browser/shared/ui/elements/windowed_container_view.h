// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_WINDOWED_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_WINDOWED_CONTAINER_VIEW_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// A WindowedContainerView is a CGRectZero sized hidden view that attaches
// itself to the application keyWindow.  Any view added as a subview will
// -resignFirstResponder to prevent a hidden view showing a keyboard.
@interface WindowedContainerView : UIView
@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_WINDOWED_CONTAINER_VIEW_H_
