// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_PASTE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_PASTE_DELEGATE_H_

#import <UIKit/UIKit.h>

// Implements UITextPasteDelegate to workaround http://crbug.com/755620.
@interface OmniboxTextFieldPasteDelegate : NSObject
@end

@interface OmniboxTextFieldPasteDelegate (
    UITextPasteDelegate)<UITextPasteDelegate>
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_PASTE_DELEGATE_H_
