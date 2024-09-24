// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"

#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

web::BrowserState* GetBrowserStateRedirectedInIncognito(
    web::BrowserState* browser_state) {
  return static_cast<ProfileIOS*>(browser_state)->GetOriginalProfile();
}

web::BrowserState* GetBrowserStateOwnInstanceInIncognito(
    web::BrowserState* browser_state) {
  return browser_state;
}
