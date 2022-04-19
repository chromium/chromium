// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_CRBUG_1316061_VIEW_H_
#define IOS_CHROME_BROWSER_UI_UTIL_CRBUG_1316061_VIEW_H_

#import <UIKit/UIKit.h>

// TODO(crbug.com/1316061): This class is just a pretext to force swiftc to
// import a bridging header, and transitively, UIKit/UIKit.h.
@interface CrBug1316061View : UIView

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_UTIL_CRBUG_1316061_VIEW_H_
