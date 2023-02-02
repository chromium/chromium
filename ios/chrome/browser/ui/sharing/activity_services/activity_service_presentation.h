// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_SERVICE_PRESENTATION_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_SERVICE_PRESENTATION_H_

// ActivityServicePresentation controls how the activity services menu is
// dismissed on screen.
@protocol ActivityServicePresentation

// Called after the activity services UI has been completed successfully.
// It is provided to allow implementors to dismiss the UI and perform cleanup.
- (void)activityServiceDidEndPresenting;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_SERVICE_PRESENTATION_H_
