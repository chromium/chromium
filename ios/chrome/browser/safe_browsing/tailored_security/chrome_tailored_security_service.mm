// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace safe_browsing {

ChromeTailoredSecurityService::ChromeTailoredSecurityService(
    ChromeBrowserState* browser_state,
    signin::IdentityManager* identity_manager)
    : TailoredSecurityService(identity_manager, browser_state->GetPrefs()),
      browser_state_(browser_state) {}

ChromeTailoredSecurityService::~ChromeTailoredSecurityService() = default;

scoped_refptr<network::SharedURLLoaderFactory>
ChromeTailoredSecurityService::GetURLLoaderFactory() {
  return browser_state_->GetSharedURLLoaderFactory();
}

}  // namespace safe_browsing
