// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_banner_overlay_request_cancel_handler.h"

#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"

#pragma mark - InfobarBannerOverlayRequestCancelHandler

InfobarBannerOverlayRequestCancelHandler::
    InfobarBannerOverlayRequestCancelHandler(
        OverlayRequest* request,
        OverlayRequestQueue* queue,
        InfoBarIOS* infobar,
        InfobarOverlayRequestInserter* inserter,
        InfobarModalCompletionNotifier* modal_completion_notifier)
    : InfobarOverlayRequestCancelHandler(request, queue, infobar),
      inserter_(inserter),
      modal_completion_observer_(this, modal_completion_notifier, infobar),
      modal_insertion_observer_(inserter, infobar, this) {
  DCHECK(inserter_);
}

InfobarBannerOverlayRequestCancelHandler::
    ~InfobarBannerOverlayRequestCancelHandler() = default;

#pragma mark Private

void InfobarBannerOverlayRequestCancelHandler::ModalCompleted() {
  // Only cancel the banner if the modal being completed was presented from this
  // banner.
  if (presenting_modal_)
    CancelRequest();
}

#pragma mark InfobarOverlayRequestCancelHandler

void InfobarBannerOverlayRequestCancelHandler::HandleReplacement(
    InfoBarIOS* replacement) {
  // If an infobar is replaced while a request for its banner is in the queue,
  // a request for the replacement's banner should be inserted in back of the
  // handler's request.
  size_t index = 0;
  bool request_found =
      GetInfobarOverlayRequestIndex(queue(), infobar(), &index);
  DCHECK(request_found);

  InsertParams params(replacement);
  params.overlay_type = InfobarOverlayType::kBanner;
  params.insertion_index = index + 1;
  params.source = InfobarOverlayInsertionSource::kInfoBarManager;
  inserter_->InsertOverlayRequest(params);
}

#pragma mark - InsertionObserver

InfobarBannerOverlayRequestCancelHandler::InsertionObserver::InsertionObserver(
    InfobarOverlayRequestInserter* inserter,
    InfoBarIOS* infobar,
    InfobarBannerOverlayRequestCancelHandler* cancel_handler)
    : cancel_handler_(cancel_handler), infobar_(infobar) {
  DCHECK(inserter);
  DCHECK(infobar);
  DCHECK(cancel_handler);
  scoped_observation_.Observe(inserter);
}

InfobarBannerOverlayRequestCancelHandler::InsertionObserver::
    ~InsertionObserver() = default;

void InfobarBannerOverlayRequestCancelHandler::InsertionObserver::
    InfobarRequestInserted(InfobarOverlayRequestInserter* inserter,
                           const InsertParams& params) {
  if (infobar_ == params.infobar &&
      params.source == InfobarOverlayInsertionSource::kBanner)
    cancel_handler_->ModalPresentedFromBanner();
}

void InfobarBannerOverlayRequestCancelHandler::InsertionObserver::
    InserterDestroyed(InfobarOverlayRequestInserter* inserter) {
  DCHECK(scoped_observation_.IsObservingSource(inserter));
  scoped_observation_.Reset();
}

#pragma mark - InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver

InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver::
    ModalCompletionObserver(
        InfobarBannerOverlayRequestCancelHandler* cancel_handler,
        InfobarModalCompletionNotifier* completion_notifier,
        InfoBarIOS* infobar)
    : cancel_handler_(cancel_handler), infobar_(infobar) {
  DCHECK(cancel_handler_);
  DCHECK(infobar_);
  DCHECK(completion_notifier);
  scoped_observation_.Observe(completion_notifier);
}

InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver::
    ~ModalCompletionObserver() = default;

void InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver::
    InfobarModalsCompleted(InfobarModalCompletionNotifier* notifier,
                           InfoBarIOS* infobar) {
  if (infobar_ == infobar) {
    cancel_handler_->ModalCompleted();
    // The cancel handler is destroyed after CancelForModalCompletion(), so no
    // code can be added after this call.
  }
}

void InfobarBannerOverlayRequestCancelHandler::ModalCompletionObserver::
    InfobarModalCompletionNotifierDestroyed(
        InfobarModalCompletionNotifier* notifier) {
  DCHECK(scoped_observation_.IsObservingSource(notifier));
  scoped_observation_.Reset();
  cancel_handler_->ModalCompleted();
  // The cancel handler is destroyed after CancelForModalCompletion(), so no
  // code can be added after this call.
}
