// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_cancel_handler.h"

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_modal_completion_notifier.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"

// A cancel handler for Infobar banner UI OverlayRequests.
class InfobarBannerOverlayRequestCancelHandler
    : public InfobarOverlayRequestCancelHandler {
 public:
  // Constructor for a handler that cancels `request` from `queue`.  `inserter`
  // is used to insert replacement requests when an infobar is replaced.
  // `modal_completion_notifier` is used to detect the completion of any modal
  // UI that was presented from the banner.
  InfobarBannerOverlayRequestCancelHandler(
      OverlayRequest* request,
      OverlayRequestQueue* queue,
      InfoBarIOS* infobar,
      InfobarOverlayRequestInserter* inserter,
      InfobarModalCompletionNotifier* modal_completion_notifier);
  ~InfobarBannerOverlayRequestCancelHandler() override;

 private:
  // Helper object to observer Infobar request insertions.
  class InsertionObserver : public InfobarOverlayRequestInserter::Observer {
   public:
    InsertionObserver(InfobarOverlayRequestInserter* inserter,
                      InfoBarIOS* infobar,
                      InfobarBannerOverlayRequestCancelHandler* cancel_handler);
    ~InsertionObserver() override;

    void InfobarRequestInserted(InfobarOverlayRequestInserter* inserter,
                                const InsertParams& params) override;
    void InserterDestroyed(InfobarOverlayRequestInserter* inserter) override;

    // The owning cancel handler.
    raw_ptr<InfobarBannerOverlayRequestCancelHandler> cancel_handler_ = nullptr;
    // The infobar for which to look for modal insertions.
    raw_ptr<InfoBarIOS> infobar_ = nullptr;
    base::ScopedObservation<InfobarOverlayRequestInserter,
                            InfobarOverlayRequestInserter::Observer>
        scoped_observation_{this};
  };

  // Helper object that triggers request cancellation for the completion of
  // modal requests created from the banner.
  class ModalCompletionObserver
      : public InfobarModalCompletionNotifier::Observer {
   public:
    ModalCompletionObserver(
        InfobarBannerOverlayRequestCancelHandler* cancel_handler,
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
    raw_ptr<InfobarBannerOverlayRequestCancelHandler> cancel_handler_ = nullptr;
    // The infobar whose modal dismissals should trigger cancellation.
    raw_ptr<InfoBarIOS> infobar_ = nullptr;
    base::ScopedObservation<InfobarModalCompletionNotifier,
                            InfobarModalCompletionNotifier::Observer>
        scoped_observation_{this};
  };

  // Indicates to the cancel handler that its banner presented a modal.
  void ModalPresentedFromBanner() { presenting_modal_ = true; }

  // Indicates that a modal completed. Only called for modal completions of
  // infobars that match the one used to configure `request`.
  void ModalCompleted();

  // InfobarOverlayRequestCancelHandler:
  void HandleReplacement(InfoBarIOS* replacement) override;

  // Whether a modal is currently being displayed from this banner.
  bool presenting_modal_ = false;
  // The inserter used to add replacement banner requests.
  raw_ptr<InfobarOverlayRequestInserter> inserter_ = nullptr;
  // The modal completion observer.
  ModalCompletionObserver modal_completion_observer_;
  // The modal insertion observer.
  InsertionObserver modal_insertion_observer_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
