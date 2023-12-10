// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_modal_overlay_request_cancel_handler.h"

#pragma mark - InfobarModalOverlayRequestCancelHandler

InfobarModalOverlayRequestCancelHandler::
    InfobarModalOverlayRequestCancelHandler(
        OverlayRequest* request,
        OverlayRequestQueue* queue,
        InfoBarIOS* infobar,
        InfobarModalCompletionNotifier* modal_completion_notifier)
    : InfobarOverlayRequestCancelHandler(request, queue, infobar),
      modal_completion_observer_(this, modal_completion_notifier, infobar) {}

InfobarModalOverlayRequestCancelHandler::
    ~InfobarModalOverlayRequestCancelHandler() = default;

#pragma mark Private

void InfobarModalOverlayRequestCancelHandler::CancelForModalCompletion() {
  CancelRequest();
}

#pragma mark - InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver

InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver::
    ModalCompletionObserver(
        InfobarModalOverlayRequestCancelHandler* cancel_handler,
        InfobarModalCompletionNotifier* completion_notifier,
        InfoBarIOS* infobar)
    : cancel_handler_(cancel_handler), infobar_(infobar) {
  DCHECK(cancel_handler_);
  DCHECK(infobar_);
  DCHECK(completion_notifier);
  scoped_observation_.Observe(completion_notifier);
}

InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver::
    ~ModalCompletionObserver() = default;

void InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver::
    InfobarModalsCompleted(InfobarModalCompletionNotifier* notifier,
                           InfoBarIOS* infobar) {
  if (infobar_ == infobar) {
    cancel_handler_->CancelForModalCompletion();
    // The cancel handler is destroyed after CancelForModalCompletion(), so no
    // code can be added after this call.
  }
}

void InfobarModalOverlayRequestCancelHandler::ModalCompletionObserver::
    InfobarModalCompletionNotifierDestroyed(
        InfobarModalCompletionNotifier* notifier) {
  DCHECK(scoped_observation_.IsObservingSource(notifier));
  scoped_observation_.Reset();
  cancel_handler_->CancelForModalCompletion();
  // The cancel handler is destroyed after CancelForModalCompletion(), so no
  // code can be added after this call.
}
