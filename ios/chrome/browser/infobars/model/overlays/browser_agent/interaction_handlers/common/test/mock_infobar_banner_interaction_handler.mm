// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/test/mock_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"

MockInfobarBannerInteractionHandler::MockInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(OverlayRequestSupport::All()) {}

MockInfobarBannerInteractionHandler::~MockInfobarBannerInteractionHandler() =
    default;
