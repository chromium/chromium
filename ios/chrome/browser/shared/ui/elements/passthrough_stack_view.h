// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_PASSTHROUGH_STACK_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_PASSTHROUGH_STACK_VIEW_H_

#import <UIKit/UIKit.h>

// A UIStackView subclass that passes touches on its transparent area through
// to whatever view is behind it.
@interface PassthroughStackView : UIStackView

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_PASSTHROUGH_STACK_VIEW_H_
