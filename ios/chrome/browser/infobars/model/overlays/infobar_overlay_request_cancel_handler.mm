// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_cancel_handler.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"

using infobars::InfoBar;
using infobars::InfoBarManager;

BASE_FEATURE(kInfobarRemoveCheck,
             "InfobarRemoveCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

#pragma mark - InfobarOverlayRequestCancelHandler

InfobarOverlayRequestCancelHandler::InfobarOverlayRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue,
    InfoBarIOS* infobar)
    : OverlayRequestCancelHandler(request, queue),
      infobar_(infobar),
      removal_observer_(this) {
  DCHECK(infobar_);
}

InfobarOverlayRequestCancelHandler::~InfobarOverlayRequestCancelHandler() =
    default;

#pragma mark - Protected

void InfobarOverlayRequestCancelHandler::HandleReplacement(
    InfoBarIOS* replacement) {}

#pragma mark - Private

void InfobarOverlayRequestCancelHandler::CancelForInfobarRemoval() {
  CancelRequest();
}

#pragma mark - InfobarOverlayRequestCancelHandler::RemovalObserver

InfobarOverlayRequestCancelHandler::RemovalObserver::RemovalObserver(
    InfobarOverlayRequestCancelHandler* cancel_handler)
    : cancel_handler_(cancel_handler) {
  DCHECK(cancel_handler_);
  InfoBarManager* manager = cancel_handler_->infobar()->owner();
  DCHECK(manager);
  scoped_observation_.Observe(manager);
}

InfobarOverlayRequestCancelHandler::RemovalObserver::~RemovalObserver() =
    default;

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
    bool animate) {
  if (cancel_handler_->infobar() == infobar) {
    if (base::FeatureList::IsEnabled(kInfobarRemoveCheck)) {
      static_cast<InfoBarIOS*>(infobar)->set_removed_from_owner();
    }
    cancel_handler_->CancelForInfobarRemoval();
    // The cancel handler is destroyed after Cancel(), so no code can be added
    // after this call.
  }
}

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnInfoBarReplaced(
    InfoBar* old_infobar,
    InfoBar* new_infobar) {
  if (cancel_handler_->infobar() == old_infobar) {
    if (base::FeatureList::IsEnabled(kInfobarRemoveCheck)) {
      static_cast<InfoBarIOS*>(old_infobar)->set_removed_from_owner();
    }
    cancel_handler_->HandleReplacement(static_cast<InfoBarIOS*>(new_infobar));
    cancel_handler_->CancelForInfobarRemoval();
    // The cancel handler is destroyed after Cancel(), so no code can be added
    // after this call.
  }
}

void InfobarOverlayRequestCancelHandler::RemovalObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK(scoped_observation_.IsObservingSource(manager));
  scoped_observation_.Reset();
  cancel_handler_->CancelForInfobarRemoval();
  // The cancel handler is destroyed after Cancel(), so no code can be added
  // after this call.
}
