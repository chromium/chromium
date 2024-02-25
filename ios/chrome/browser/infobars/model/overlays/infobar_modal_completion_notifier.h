// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_MODAL_COMPLETION_NOTIFIER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_MODAL_COMPLETION_NOTIFIER_H_

#include <map>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"

class InfoBarIOS;
class OverlayRequestQueueCallbackInstaller;
namespace web {
class WebState;
}

// Helper object that notifies observers when all modal OverlayRequests for an
// infobar have been completed.
class InfobarModalCompletionNotifier {
 public:
  // Observer class to notify objects of when the modal requests for an infobar
  // have been completed.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Called to notify observers of `notifier` that the modal requests for
    // `infobar` have completed.  Banners should remain visible until all
    // requests for modal UI originating from the banner's infobar is completed.
    virtual void InfobarModalsCompleted(
        InfobarModalCompletionNotifier* notifier,
        InfoBarIOS* infobar) {}

    // Called when `notifier` is being destroyed.
    virtual void InfobarModalCompletionNotifierDestroyed(
        InfobarModalCompletionNotifier* notifier) {}
  };

  // Constructs a notifier that observes the completion of modal requests in
  // `web_state`'s queue.
  explicit InfobarModalCompletionNotifier(web::WebState* web_state);
  ~InfobarModalCompletionNotifier();

  // Adds and removes observers of the cancellation dependency state.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Helper object that observes presentation of infobar overlay UI to detect
  // the dismissal of modals.
  class ModalCompletionInstaller : public OverlayRequestCallbackInstaller {
   public:
    explicit ModalCompletionInstaller(InfobarModalCompletionNotifier* notifier);
    ~ModalCompletionInstaller() override;

   private:
    // Used as a completion callback for the modal OverlayRequests for
    // `infobar`.
    void ModalCompleted(InfoBarIOS* infobar, OverlayResponse* response);

    // OverlayRequestCallbackInstaller:
    const OverlayRequestSupport* GetRequestSupport() const override;
    void InstallCallbacksInternal(OverlayRequest* request) override;

    // The owning notifier.
    raw_ptr<InfobarModalCompletionNotifier> notifier_ = nullptr;
    base::WeakPtrFactory<ModalCompletionInstaller> weak_factory_;
  };

  // Called when a completion callback for a modal request for `infobar` has
  // been installed.
  void ModalCompletionInstalled(InfoBarIOS* infobar);

  // Called when a modal request for `infobar` has been completed.
  void ModalRequestCompleted(InfoBarIOS* infobar);

  // Map storing the number of active modal OverlayRequests for a given
  // InfoBarIOS.
  std::map<InfoBarIOS*, size_t> active_modal_request_counts_;
  // The queue callback installer that adds completion callbacks to infobar
  // modal requests.
  std::unique_ptr<OverlayRequestQueueCallbackInstaller> callback_installer_;
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_MODAL_COMPLETION_NOTIFIER_H_
