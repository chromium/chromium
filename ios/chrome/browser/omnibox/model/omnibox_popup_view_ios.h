// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"

@class OmniboxAutocompleteController;
class OmniboxEditModelIOS;

class OmniboxPopupViewIOS {
 public:
  OmniboxPopupViewIOS(
      OmniboxEditModelIOS* omnibox_edit_model,
      OmniboxAutocompleteController* omnibox_autocomplete_controller);
  virtual ~OmniboxPopupViewIOS();

  // Returns true if the popup is currently open.
  virtual bool IsOpen() const;

 private:
  base::WeakPtr<OmniboxEditModelIOS> model_;

  __weak OmniboxAutocompleteController* omnibox_autocomplete_controller_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_IOS_H_
