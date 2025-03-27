// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_TEXT_CHANGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_TEXT_CHANGE_DELEGATE_H_

#import <UIKit/UIKit.h>

// The delegate that is notified of the return key being pressed in the omnibox
// Equivalent to OmniboxTextAcceptDelegate but in Obj-C
@protocol OmniboxReturnDelegate
// Called when the Omnibox text field returns. (The "go" button is tapped.)
- (void)omniboxReturnPressed:(id)sender;

@end

// Equivalent to OmniboxReturnDelegate but in C++
class OmniboxTextAcceptDelegate {
 public:
  // Hide keyboard and call OnDidEndEditing.  This dismisses the keyboard and
  // also finalizes the editing sta
  // Called when the Omnibox text field returns. (The "go" button is tapped.)
  virtual void OnAccept() = 0;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_TEXT_CHANGE_DELEGATE_H_
