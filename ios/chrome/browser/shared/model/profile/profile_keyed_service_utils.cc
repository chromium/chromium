// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_utils.h"

#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/web/public/browser_state.h"

web::BrowserState* GetContextToUseForKeyedServiceFactory(
    web::BrowserState* context,
    ProfileSelection profile_selection) {
  if (!context) {
    return nullptr;
  }

  if (!context->IsOffTheRecord()) {
    return context;
  }

  switch (profile_selection) {
    case ProfileSelection::kNoInstanceInIncognito:
      return nullptr;

    case ProfileSelection::kRedirectedInIncognito:
      return ProfileIOS::FromBrowserState(context)->GetOriginalProfile();

    case ProfileSelection::kOwnInstanceInIncognito:
      return context;
  }
}
