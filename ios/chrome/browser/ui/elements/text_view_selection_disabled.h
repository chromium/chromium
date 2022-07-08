// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ELEMENTS_TEXT_VIEW_SELECTION_DISABLED_H_
#define IOS_CHROME_BROWSER_UI_ELEMENTS_TEXT_VIEW_SELECTION_DISABLED_H_

#import <UIKit/UIKit.h>

// UITextView subclass is needed to override -canBecomeFirstResponder to prevent
// text selection while maintaining clickable link.
@interface TextViewSelectionDisabled : UITextView

// Creates and returns a TextViewSelectionDisabled with workarounds
// for iOS 16 link tap issues.
+ (TextViewSelectionDisabled*)textView;

@end

#endif  // IOS_CHROME_BROWSER_UI_ELEMENTS_TEXT_VIEW_SELECTION_DISABLED_H_
