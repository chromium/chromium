// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

web::BrowserState* GetBrowserStateRedirectedInIncognito(
    web::BrowserState* browser_state) {
  return static_cast<ChromeBrowserState*>(browser_state)
      ->GetOriginalChromeBrowserState();
}

web::BrowserState* GetBrowserStateOwnInstanceInIncognito(
    web::BrowserState* browser_state) {
  return browser_state;
}
