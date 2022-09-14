// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_H_

#include "components/omnibox/browser/omnibox_edit_controller.h"

namespace web {
class WebState;
}

// iOS-specific extension of the OmniboxEditController base class.
class WebOmniboxEditController : public OmniboxEditController {
 public:
  WebOmniboxEditController(const WebOmniboxEditController&) = delete;
  WebOmniboxEditController& operator=(const WebOmniboxEditController&) = delete;

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
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_H_
