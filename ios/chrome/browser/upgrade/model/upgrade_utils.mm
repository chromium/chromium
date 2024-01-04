// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/model/upgrade_utils.h"

#import <Foundation/Foundation.h>

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"

bool IsAppUpToDate() {
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  return prefService->GetBoolean(kIOSChromeUpToDateKey);
}
