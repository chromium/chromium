// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"

#import "base/check.h"

InfobarInteractionHandler::InfobarInteractionHandler(
    InfobarType infobar_type,
    std::unique_ptr<Handler> banner_handler,
    std::unique_ptr<Handler> modal_handler)
    : infobar_type_(infobar_type),
      banner_handler_(std::move(banner_handler)),
      modal_handler_(std::move(modal_handler)) {
  DCHECK(banner_handler_);
}

InfobarInteractionHandler::~InfobarInteractionHandler() = default;

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarInteractionHandler::CreateBannerCallbackInstaller() {
  return banner_handler_->CreateInstaller();
}

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarInteractionHandler::CreateModalCallbackInstaller() {
  return modal_handler_ ? modal_handler_->CreateInstaller() : nullptr;
}

void InfobarInteractionHandler::InfobarVisibilityChanged(
    InfoBarIOS* infobar,
    InfobarOverlayType overlay_type,
    bool visible) {
  Handler* handler = nullptr;
  switch (overlay_type) {
    case InfobarOverlayType::kBanner:
      handler = banner_handler_.get();
      break;
    case InfobarOverlayType::kModal:
      handler = modal_handler_.get();
      break;
  }
  if (handler)
    handler->InfobarVisibilityChanged(infobar, visible);
}
