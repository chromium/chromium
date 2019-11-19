// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_IMPL_H_

#include "base/memory/weak_ptr.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"
#import "ios/chrome/browser/overlays/public/overlay_dismissal_callback.h"
#import "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/public/overlay_presentation_context_observer.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

// Implementation of OverlayPresenter.  The presenter:
// - observes OverlayRequestQueue modifications for the active WebState and
//   triggers the presentation for added requests using the UI delegate.
// - manages hiding and showing overlays for active WebState changes.
class OverlayPresenterImpl : public BrowserObserver,
                             public OverlayPresenter,
                             public OverlayPresentationContextObserver,
                             public OverlayRequestQueueImpl::Observer,
                             public WebStateListObserver {
 public:
  ~OverlayPresenterImpl() override;

  // Container that stores the presenters for each modality.  Usage example:
  //
  // OverlayPresenterImpl::Container::FromUserData(browser)->
  //     PresenterForModality(OverlayModality::kWebContentArea);
  class Container : public OverlayUserData<Container> {
   public:
    ~Container() override;

    // Returns the OverlayPresenterImpl for |modality|.
    OverlayPresenterImpl* PresenterForModality(OverlayModality modality);

   private:
    OVERLAY_USER_DATA_SETUP(Container);
    explicit Container(Browser* browser);

    Browser* browser_ = nullptr;
    std::map<OverlayModality, std::unique_ptr<OverlayPresenterImpl>>
        presenters_;
  };

  // OverlayPresenter:
  void SetPresentationContext(
      OverlayPresentationContext* presentation_context) override;
  void AddObserver(OverlayPresenterObserver* observer) override;
  void RemoveObserver(OverlayPresenterObserver* observer) override;
  bool IsShowingOverlayUI() const override;

 private:
  // Private constructor used by the container.
  OverlayPresenterImpl(Browser* browser, OverlayModality modality);

  // Setter for the active WebState.  Setting to a new value will hide any
  // presented overlays and show the next overlay for the new active WebState.
  void SetActiveWebState(web::WebState* web_state,
                         WebStateListObserver::ChangeReason reason);

  // Fetches the request queue for |web_state|, creating it if necessary.
  OverlayRequestQueueImpl* GetQueueForWebState(web::WebState* web_state) const;

  // Returns the front request for |web_state|'s request queue.
  OverlayRequest* GetFrontRequestForWebState(web::WebState* web_state) const;

  // Returns the request queue for the active WebState.
  OverlayRequestQueueImpl* GetActiveQueue() const;

  // Returns the front request for the active queue.
  OverlayRequest* GetActiveRequest() const;

  // Triggers the presentation of the overlay UI for the active request.  Does
  // nothing if there is no active request or if there is no UI delegate.  Must
  // only be called when |presenting_| is false.
  void PresentOverlayForActiveRequest();

  // Notifies this object that the UI for |request| has finished being
  // presented in |presentation_context|.  This function is called when the
  // OverlayPresentationCallback provided to the presentation context is
  // executed.
  void OverlayWasPresented(OverlayPresentationContext* presentation_context,
                           OverlayRequest* request);

  // Notifies this object that the UI for |request| has finished being dismissed
  // in |presentation_context| in for |reason|.  |queue| is |request|'s queue.
  // This function is called when the OverlayDismissalCallback provided to
  // |presentation_context| is executed.
  void OverlayWasDismissed(OverlayPresentationContext* presentation_context,
                           OverlayRequest* request,
                           base::WeakPtr<OverlayRequestQueueImpl> queue,
                           OverlayDismissalReason reason);

  // Cancels all overlays for |request|.
  void CancelOverlayUIForRequest(OverlayRequest* request);

  // Cancels all overlays for the Browser.
  void CancelAllOverlayUI();

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

  // OverlayRequestQueueImpl::Observer:
  void RequestAddedToQueue(OverlayRequestQueueImpl* queue,
                           OverlayRequest* request) override;
  void QueuedRequestCancelled(OverlayRequestQueueImpl* queue,
                              OverlayRequest* request) override;

  // OverlayPresentationContextObserver:
  void OverlayPresentationContextWillChangePresentationCapabilities(
      OverlayPresentationContext* presentation_context,
      OverlayPresentationContext::UIPresentationCapabilities capabilities)
      override;
  void OverlayPresentationContextDidChangePresentationCapabilities(
      OverlayPresentationContext* presentation_context) override;

  // WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WillDetachWebStateAt(WebStateList* web_state_list,
                            web::WebState* web_state,
                            int index) override;
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           int reason) override;

  // Whether the UI delegate is presenting overlay UI for this presenter.  Stays
  // true from the beginning of the presentation until the end of the
  // dismissal.
  bool presenting_ = false;
  // Whether the active WebState is being detached.
  bool detaching_active_web_state_ = false;

  OverlayModality modality_;
  WebStateList* web_state_list_ = nullptr;
  web::WebState* active_web_state_ = nullptr;
  OverlayPresentationContext* presentation_context_ = nullptr;
  base::ObserverList<OverlayPresenterObserver,
                     /* check_empty= */ true>
      observers_;
  base::WeakPtrFactory<OverlayPresenterImpl> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_PRESENTER_IMPL_H_
