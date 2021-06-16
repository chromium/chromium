// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_account_manager_service.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/check.h"

ChromeAccountManagerService::ChromeAccountManagerService(
    PrefService* pref_service) {
  DCHECK(pref_service);
}

void ChromeAccountManagerService::Shutdown() {}
