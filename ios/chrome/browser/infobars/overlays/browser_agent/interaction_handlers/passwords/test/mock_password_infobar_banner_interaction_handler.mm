// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/test/mock_password_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/overlays/public/overlay_request_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

MockPasswordInfobarBannerInteractionHandler::
    MockPasswordInfobarBannerInteractionHandler(
        Browser* browser,
        password_modal::PasswordAction action_type)
    : PasswordInfobarBannerInteractionHandler(browser,
                                              action_type,
                                              OverlayRequestSupport::All()) {}

MockPasswordInfobarBannerInteractionHandler::
    ~MockPasswordInfobarBannerInteractionHandler() = default;
