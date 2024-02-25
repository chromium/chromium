// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"

#import "base/check_op.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_banner_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_modal_completion_notifier.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_modal_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_factory.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"

WEB_STATE_USER_DATA_KEY_IMPL(InfobarOverlayRequestInserter)

InsertParams::InsertParams(InfoBarIOS* infobar) : infobar(infobar) {}

InfobarOverlayRequestInserter::InfobarOverlayRequestInserter(
    web::WebState* web_state,
    InfobarOverlayRequestFactory factory)
    : web_state_(web_state),
      modal_completion_notifier_(
          std::make_unique<InfobarModalCompletionNotifier>(web_state_)),
      request_factory_(factory) {
  DCHECK(web_state_);
  DCHECK(request_factory_);
  // Populate `queues_` with the request queues at the appropriate modalities.
  queues_[InfobarOverlayType::kBanner] = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kInfobarBanner);
  queues_[InfobarOverlayType::kModal] = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kInfobarModal);
}

InfobarOverlayRequestInserter::~InfobarOverlayRequestInserter() {
  for (auto& observer : observers_) {
    observer.InserterDestroyed(this);
  }
}

void InfobarOverlayRequestInserter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}
void InfobarOverlayRequestInserter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InfobarOverlayRequestInserter::InsertOverlayRequest(
    const InsertParams& params) {
  // Create the request and its cancel handler.
  std::unique_ptr<OverlayRequest> request =
      request_factory_(params.infobar, params.overlay_type);
  DCHECK(request);
  DCHECK_EQ(params.infobar,
            request->GetConfig<InfobarOverlayRequestConfig>()->infobar());
  OverlayRequestQueue* queue = queues_.at(params.overlay_type);
  std::unique_ptr<OverlayRequestCancelHandler> cancel_handler;
  switch (params.overlay_type) {
    case InfobarOverlayType::kBanner:
      cancel_handler =
          std::make_unique<InfobarBannerOverlayRequestCancelHandler>(
              request.get(), queue, params.infobar, this,
              modal_completion_notifier_.get());
      break;
    case InfobarOverlayType::kModal:
      // Add placeholder request in front of banner queue so no banner get
      // presented behind the modal.
      cancel_handler = std::make_unique<InfobarOverlayRequestCancelHandler>(
          request.get(), queue, params.infobar);
      OverlayRequestQueue* banner_queue =
          queues_.at(InfobarOverlayType::kBanner);
      std::unique_ptr<OverlayRequest> placeholder_request =
          OverlayRequest::CreateWithConfig<
              InfobarBannerPlaceholderRequestConfig>(params.infobar);
      std::unique_ptr<InfobarModalOverlayRequestCancelHandler>
          modal_cancel_handler =
              std::make_unique<InfobarModalOverlayRequestCancelHandler>(
                  placeholder_request.get(), banner_queue, params.infobar,
                  modal_completion_notifier_.get());
      banner_queue->InsertRequest(0, std::move(placeholder_request),
                                  std::move(modal_cancel_handler));
      break;
  }
  for (auto& observer : observers_) {
    observer.InfobarRequestInserted(this, params);
  }
  queue->InsertRequest(params.insertion_index, std::move(request),
                       std::move(cancel_handler));
}
