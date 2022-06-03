// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/test/mock_infobar_banner_interaction_handler.h"

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

MockInfobarBannerInteractionHandler::MockInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(OverlayRequestSupport::All()) {}

MockInfobarBannerInteractionHandler::~MockInfobarBannerInteractionHandler() =
    default;
