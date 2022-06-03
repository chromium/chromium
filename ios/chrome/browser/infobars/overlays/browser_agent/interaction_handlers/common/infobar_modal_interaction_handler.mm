// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_interaction_handler.h"

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

InfobarModalInteractionHandler::InfobarModalInteractionHandler() = default;

InfobarModalInteractionHandler::~InfobarModalInteractionHandler() = default;

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarModalInteractionHandler::CreateInstaller() {
  return CreateModalInstaller();
}
