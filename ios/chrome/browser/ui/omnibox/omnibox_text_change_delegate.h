// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_CHANGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_CHANGE_DELEGATE_H_

#import <UIKit/UIKit.h>

class OmniboxTextChangeDelegate {
 public:
  // Called when the Omnibox text field starts editing
  virtual void OnDidBeginEditing() = 0;
  // Called before the Omnibox text field changes. |new_text| will replace the
  // text currently in |range|. This should return true if the text change
  // should happen and false otherwise.
  // See -textField:shouldChangeCharactersInRange:replacementString: for more
  // details.
  virtual bool OnWillChange(NSRange range, NSString* new_text) = 0;
  // Called after the Omnibox text field changes. |processing_user_input| holds
  // whether the change was user-initiated or programmatic.
  virtual void OnDidChange(bool processing_user_input) = 0;
  // Called before the Omnibox text field finishes editing.
  virtual void OnWillEndEditing() = 0;
  // Hide keyboard and call OnDidEndEditing.  This dismisses the keyboard and
  // also finalizes the editing state of the omnibox.
  virtual void EndEditing() = 0;
  // Called when the Omnibox text field returns. (The "go" button is tapped.)
  virtual void OnAccept() = 0;
  // Called when the Omnibox text field should copy.
  virtual void OnCopy() = 0;
  // Clear the Omnibox text.
  virtual void ClearText() = 0;
  // Called when the Omnibox text field should paste.
  virtual void WillPaste() = 0;
  // Called when the backspace button is pressed in the Omnibox text field.
  virtual void OnDeleteBackward() = 0;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_CHANGE_DELEGATE_H_
