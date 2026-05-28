// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_CHROME_ACTIVITY_H_
#define IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_CHROME_ACTIVITY_H_

#import <UIKit/UIKit.h>

// A UIActivity base class that adds common Chrome integration capability.
@interface ChromeActivity : UIActivity

// Disconnects the activity from any C++ backing objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_CHROME_ACTIVITY_H_
