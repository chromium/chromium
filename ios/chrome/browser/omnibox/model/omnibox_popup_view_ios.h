// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"

@class OmniboxAutocompleteController;
class OmniboxControllerIOS;
class OmniboxEditModelIOS;

class OmniboxPopupViewIOS {
 public:
  OmniboxPopupViewIOS(
      OmniboxControllerIOS* controller,
      OmniboxAutocompleteController* omnibox_autocomplete_controller);
  virtual ~OmniboxPopupViewIOS();

  OmniboxEditModelIOS* model();
  const OmniboxEditModelIOS* model() const;

  OmniboxControllerIOS* controller();
  const OmniboxControllerIOS* controller() const;

  // Returns true if the popup is currently open.
  virtual bool IsOpen() const;

  // Redraws the popup window to match any changes in the result set; this may
  // mean opening or closing the window.
  virtual void UpdatePopupAppearance();

 private:
  // Owned by OmniboxViewIOS which owns this.
  const raw_ptr<OmniboxControllerIOS> controller_;

  __weak OmniboxAutocompleteController* omnibox_autocomplete_controller_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_IOS_H_
