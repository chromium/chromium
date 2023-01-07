// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

class LocationBarModel;

// Protocol receiving notification when the some events occur in the
// ToolbarCoordinator
@protocol ToolbarCoordinatorDelegate<NSObject>

// Called when the location bar gains keyboard focus.
- (void)locationBarDidBecomeFirstResponder;
// Called when the location bar loses keyboard focus.
- (void)locationBarDidResignFirstResponder;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_COORDINATOR_DELEGATE_H_
