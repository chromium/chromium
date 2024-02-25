// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_modal_interaction_handler.h"

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

InfobarModalInteractionHandler::InfobarModalInteractionHandler() = default;

InfobarModalInteractionHandler::~InfobarModalInteractionHandler() = default;

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarModalInteractionHandler::CreateInstaller() {
  return CreateModalInstaller();
}
