// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_H_

#include "base/macros.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"

namespace web {
class WebState;
}

// iOS-specific extension of the OmniboxEditController base class.
class WebOmniboxEditController : public OmniboxEditController {
 public:
  // Returns the WebState of the currently active tab.
  virtual web::WebState* GetWebState() = 0;

  // The autocomplete edit lost focus.
  virtual void OnKillFocus() = 0;

  // The autocomplete got focus. In UI Refresh, this is not called if the popup
  // was already open when the omnibox is refocused.
  virtual void OnSetFocus() = 0;

 protected:
  WebOmniboxEditController();
  ~WebOmniboxEditController() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebOmniboxEditController);
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_H_
