// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/translate_infobar_placeholder_overlay_request_cancel_handler.h"

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/common/placeholder_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_cancel_handler.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/web/public/web_state.h"

namespace translate_infobar_overlays {

PlaceholderRequestCancelHandler::PlaceholderRequestCancelHandler(
    OverlayRequest* request,
    OverlayRequestQueue* queue,
    TranslateOverlayTabHelper* tab_helper,
    InfoBarIOS* translate_infobar)
    : InfobarOverlayRequestCancelHandler(request, queue, translate_infobar),
      translation_finished_observer_(tab_helper, this) {}

PlaceholderRequestCancelHandler::~PlaceholderRequestCancelHandler() = default;

void PlaceholderRequestCancelHandler::TranslationHasFinished() {
  CancelRequest();
}

#pragma mark - TranslationFinishedObserver

PlaceholderRequestCancelHandler::TranslationFinishedObserver::
    TranslationFinishedObserver(TranslateOverlayTabHelper* tab_helper,
                                PlaceholderRequestCancelHandler* cancel_handler)
    : cancel_handler_(cancel_handler) {
  scoped_observation_.Observe(tab_helper);
}

PlaceholderRequestCancelHandler::TranslationFinishedObserver::
    ~TranslationFinishedObserver() = default;

void PlaceholderRequestCancelHandler::TranslationFinishedObserver::
    TranslationFinished(TranslateOverlayTabHelper* tab_helper, bool success) {
  cancel_handler_->TranslationHasFinished();
}

void PlaceholderRequestCancelHandler::TranslationFinishedObserver::
    TranslateOverlayTabHelperDestroyed(TranslateOverlayTabHelper* tab_helper) {
  DCHECK(scoped_observation_.IsObservingSource(tab_helper));
  scoped_observation_.Reset();
}

}  // namespace translate_infobar_overlays
