// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_tab_helper.h"

#import "base/check.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"

using infobars::InfoBar;
using infobars::InfoBarManager;

#pragma mark - InfobarOverlayTabHelper

WEB_STATE_USER_DATA_KEY_IMPL(InfobarOverlayTabHelper)

InfobarOverlayTabHelper::InfobarOverlayTabHelper(web::WebState* web_state)
    : request_inserter_(InfobarOverlayRequestInserter::FromWebState(web_state)),
      request_scheduler_(web_state, this) {}

InfobarOverlayTabHelper::~InfobarOverlayTabHelper() = default;

#pragma mark - InfobarOverlayTabHelper::OverlayRequestScheduler

InfobarOverlayTabHelper::OverlayRequestScheduler::OverlayRequestScheduler(
    web::WebState* web_state,
    InfobarOverlayTabHelper* tab_helper)
    : tab_helper_(tab_helper), web_state_(web_state) {
  DCHECK(tab_helper_);
  InfoBarManager* manager = InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(manager);
  scoped_observation_.Observe(manager);
}

InfobarOverlayTabHelper::OverlayRequestScheduler::~OverlayRequestScheduler() =
    default;

void InfobarOverlayTabHelper::OverlayRequestScheduler::OnInfoBarAdded(
    InfoBar* infobar) {
  InfoBarIOS* ios_infobar = static_cast<InfoBarIOS*>(infobar);
  // Skip showing banner if it was requested. Badge and modals will keep
  // showing.
  if (ios_infobar->skip_banner())
    return;
  InsertParams params(ios_infobar);
  params.overlay_type = InfobarOverlayType::kBanner;
  // If the Infobar high priority, then insert it into the front of the banner
  // queue.
  params.insertion_index =
      ios_infobar->high_priority()
          ? 0
          : OverlayRequestQueue::FromWebState(web_state_,
                                              OverlayModality::kInfobarBanner)
                ->size();
  params.source = InfobarOverlayInsertionSource::kInfoBarManager;
  tab_helper_->request_inserter()->InsertOverlayRequest(params);
}

void InfobarOverlayTabHelper::OverlayRequestScheduler::OnManagerShuttingDown(
    InfoBarManager* manager) {
  DCHECK(scoped_observation_.IsObservingSource(manager));
  scoped_observation_.Reset();
}
