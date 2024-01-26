// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_MODAL_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_MODAL_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_cancel_handler.h"

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_modal_completion_notifier.h"

// A cancel handler for Infobar modal UI OverlayRequests.
class InfobarModalOverlayRequestCancelHandler
    : public InfobarOverlayRequestCancelHandler {
 public:
  // Constructor for a handler that cancels `request` from `queue`.
  // `modal_completion_notifier` is used to detect the completion of any modal
  // UI that was presented from the banner.
  InfobarModalOverlayRequestCancelHandler(
      OverlayRequest* request,
      OverlayRequestQueue* queue,
      InfoBarIOS* infobar,
      InfobarModalCompletionNotifier* modal_completion_notifier);
  ~InfobarModalOverlayRequestCancelHandler() override;

 private:
  // Helper object that triggers request cancellation when the modal dismisses.
  class ModalCompletionObserver
      : public InfobarModalCompletionNotifier::Observer {
   public:
    ModalCompletionObserver(
        InfobarModalOverlayRequestCancelHandler* cancel_handler,
        InfobarModalCompletionNotifier* completion_notifier,
        InfoBarIOS* infobar);
    ~ModalCompletionObserver() override;

   private:
    // InfobarModalCompletionNotifier::Observer:
    void InfobarModalsCompleted(InfobarModalCompletionNotifier* notifier,
                                InfoBarIOS* infobar) override;
    void InfobarModalCompletionNotifierDestroyed(
        InfobarModalCompletionNotifier* notifier) override;

    // The owning cancel handler.
    raw_ptr<InfobarModalOverlayRequestCancelHandler> cancel_handler_ = nullptr;
    // The infobar whose modal dismissals should trigger cancellation.
    raw_ptr<InfoBarIOS> infobar_ = nullptr;
    base::ScopedObservation<InfobarModalCompletionNotifier,
                            InfobarModalCompletionNotifier::Observer>
        scoped_observation_{this};
  };

  // Cancels the request for modal completion.
  void CancelForModalCompletion();

  // The modal completion completion observer.
  ModalCompletionObserver modal_completion_observer_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_MODAL_OVERLAY_REQUEST_CANCEL_HANDLER_H_
