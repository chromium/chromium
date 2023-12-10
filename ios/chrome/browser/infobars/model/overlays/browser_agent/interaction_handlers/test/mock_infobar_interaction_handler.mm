// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/test/mock_infobar_interaction_handler.h"

#import "base/check.h"

#pragma mark - MockInfobarInteractionHandler::Handler

MockInfobarInteractionHandler::Handler::Handler() = default;
MockInfobarInteractionHandler::Handler::~Handler() = default;

#pragma mark - MockInfobarInteractionHandler::Builder

MockInfobarInteractionHandler::Builder::Builder(InfobarType infobar_type)
    : infobar_type_(infobar_type) {}

MockInfobarInteractionHandler::Builder::~Builder() = default;

std::unique_ptr<InfobarInteractionHandler>
MockInfobarInteractionHandler::Builder::Build() {
  DCHECK(!has_built_);
  has_built_ = true;

  std::unique_ptr<MockInfobarInteractionHandler::Handler> banner_handler =
      std::make_unique<MockInfobarInteractionHandler::Handler>();
  std::unique_ptr<MockInfobarInteractionHandler::Handler> modal_handler =
      std::make_unique<MockInfobarInteractionHandler::Handler>();
  mock_handlers_[InfobarOverlayType::kBanner] = banner_handler.get();
  mock_handlers_[InfobarOverlayType::kModal] = modal_handler.get();
  return std::make_unique<InfobarInteractionHandler>(
      infobar_type_, std::move(banner_handler), std::move(modal_handler));
}
