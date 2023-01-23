// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_MODEL_DELEGATE_H_

#include "components/omnibox/browser/omnibox_edit_model_delegate.h"

namespace web {
class WebState;
}

// iOS-specific extension of the OmniboxEditModelDelegate base class.
class WebOmniboxEditModelDelegate : public OmniboxEditModelDelegate {
 public:
  WebOmniboxEditModelDelegate(const WebOmniboxEditModelDelegate&) = delete;
  WebOmniboxEditModelDelegate& operator=(const WebOmniboxEditModelDelegate&) =
      delete;

  // Returns the WebState of the currently active tab.
  virtual web::WebState* GetWebState() = 0;

  // The autocomplete edit lost focus.
  virtual void OnKillFocus() = 0;

  // The autocomplete got focus. In UI Refresh, this is not called if the popup
  // was already open when the omnibox is refocused.
  virtual void OnSetFocus() = 0;

 protected:
  WebOmniboxEditModelDelegate();
  ~WebOmniboxEditModelDelegate() override;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_MODEL_DELEGATE_H_
