// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_REUSE_CHECK_SERVICE_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_REUSE_CHECK_SERVICE_INTERNAL_H_

#import "ios/web_view/internal/passwords/web_view_affiliation_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_reuse_check_service.h"

@interface CWVReuseCheckService ()

- (instancetype)initWithAffiliationService:
    (password_manager::AffiliationService*)affiliationService
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_REUSE_CHECK_SERVICE_INTERNAL_H_
