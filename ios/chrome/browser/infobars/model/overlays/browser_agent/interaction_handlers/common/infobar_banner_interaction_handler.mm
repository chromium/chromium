// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

#import "base/check.h"
#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_banner_overlay_request_callback_installer.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"

namespace {
infobars::InfoBarDelegate* GetInfobarDelegate(InfoBarIOS* infobar) {
  infobars::InfoBarDelegate* delegate = infobar->delegate();
  DCHECK(delegate);
  return delegate;
}
}  // namespace

InfobarBannerInteractionHandler::InfobarBannerInteractionHandler(
    const OverlayRequestSupport* request_support)
    : request_support_(request_support) {
  DCHECK(request_support_);
}

InfobarBannerInteractionHandler::~InfobarBannerInteractionHandler() = default;

std::unique_ptr<OverlayRequestCallbackInstaller>
InfobarBannerInteractionHandler::CreateInstaller() {
  return CreateBannerInstaller();
}

void InfobarBannerInteractionHandler::ShowModalButtonTapped(
    InfoBarIOS* infobar,
    web::WebState* web_state) {
  InsertParams params(infobar);
  params.infobar = infobar;
  params.overlay_type = InfobarOverlayType::kModal;
  params.insertion_index = OverlayRequestQueue::FromWebState(
                               web_state, OverlayModality::kInfobarModal)
                               ->size();
  params.source = InfobarOverlayInsertionSource::kBanner;
  InfobarOverlayRequestInserter::FromWebState(web_state)->InsertOverlayRequest(
      params);
}

void InfobarBannerInteractionHandler::BannerDismissedByUser(
    InfoBarIOS* infobar) {
  // Notify the delegate that a user-initiated dismissal has been triggered.
  // NOTE: InfoBarDismissed() (camel cased) is used to notify the delegate that
  // the user initiated the upcoming dismissal (i.e. swiped to dismiss in the
  // refresh UI).  InfobarDismissed() (not camel cased) is called in
  // BannerVisibilityChanged() to notify the delegate of the dismissal of the
  // UI.
  GetInfobarDelegate(infobar)->InfoBarDismissed();
}

void InfobarBannerInteractionHandler::InfobarVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  BannerVisibilityChanged(infobar, visible);
}

std::unique_ptr<InfobarBannerOverlayRequestCallbackInstaller>
InfobarBannerInteractionHandler::CreateBannerInstaller() {
  return std::make_unique<InfobarBannerOverlayRequestCallbackInstaller>(
      request_support_, this);
}
