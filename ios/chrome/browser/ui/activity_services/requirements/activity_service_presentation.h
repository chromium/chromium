// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_PRESENTATION_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_PRESENTATION_H_

// ActivityServicePresentation controls how the activity services menu is
// dismissed on screen.
@protocol ActivityServicePresentation

// Called after the activity services UI has been dismissed.  The UIKit-provided
// UIViewController dismisses itself automatically, so the UI does not need to
// be dismissed in this method.  Instead, it is provided to allow implementors
// to perform cleanup after the UI is gone.
- (void)activityServiceDidEndPresenting;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_REQUIREMENTS_ACTIVITY_SERVICE_PRESENTATION_H_
