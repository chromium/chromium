// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_PRESENTER_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_PRESENTER_IMPL_H_

#include <set>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/overlays/model/overlay_request_queue_impl.h"
#import "ios/chrome/browser/overlays/model/public/overlay_dismissal_callback.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context_observer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_user_data.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class OverlayResponse;

// Implementation of OverlayPresenter.  The presenter:
// - observes OverlayRequestQueue modifications for the active WebState and
//   triggers the presentation for added requests using the UI delegate.
// - manages hiding and showing overlays for active WebState changes.
class OverlayPresenterImpl : public BrowserObserver,
                             public OverlayPresenter,
                             public OverlayPresentationContextObserver,
                             public OverlayRequestQueueImpl::Delegate,
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

    // Returns the OverlayPresenterImpl for `modality`.
    OverlayPresenterImpl* PresenterForModality(OverlayModality modality);

   private:
    OVERLAY_USER_DATA_SETUP(Container);
    explicit Container(Browser* browser);

    raw_ptr<Browser> browser_ = nullptr;
    std::map<OverlayModality, std::unique_ptr<OverlayPresenterImpl>>
        presenters_;
  };

  // OverlayPresenter:
  OverlayModality GetModality() const override;
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
  void SetActiveWebState(web::WebState* web_state, bool is_replaced);

  // Fetches the request queue for `web_state`, creating it if necessary.
  OverlayRequestQueueImpl* GetQueueForWebState(web::WebState* web_state) const;

  // Returns the front request for `web_state`'s request queue.
  OverlayRequest* GetFrontRequestForWebState(web::WebState* web_state) const;

  // Returns the request queue for the active WebState.
  OverlayRequestQueueImpl* GetActiveQueue() const;

  // Returns the front request for the active queue.
  OverlayRequest* GetActiveRequest() const;

  // Triggers the presentation of the overlay UI for the active request.  Does
  // nothing if there is no active request or if there is no UI delegate.  Must
  // only be called when `presenting_` is false.
  void PresentOverlayForActiveRequest();

  // Notifies this object that the UI for `request` has finished being
  // presented in `presentation_context`.  This function is called when the
  // OverlayPresentationCallback provided to the presentation context is
  // executed.
  void OverlayWasPresented(OverlayPresentationContext* presentation_context,
                           OverlayRequest* request);

  // Notifies this object that the UI for `request` has finished being dismissed
  // in `presentation_context` in for `reason`.  `queue` is `request`'s queue.
  // This function is called when the OverlayDismissalCallback provided to
  // `presentation_context` is executed.
  void OverlayWasDismissed(OverlayPresentationContext* presentation_context,
                           OverlayRequest* request,
                           base::WeakPtr<OverlayRequestQueueImpl> queue,
                           OverlayDismissalReason reason);

  // Used as a completion callback for `request`.  Cleans up state associated
  // with `request`.
  void OverlayWasCompleted(OverlayRequest* request, OverlayResponse* response);

  // Cancels all overlays for `request`.
  void CancelOverlayUIForRequest(OverlayRequest* request);

  // Cancels all overlays for the Browser.
  void CancelAllOverlayUI();

  // Sets up and tears down observation and delegation for `web_state`'s request
  // queue when it is added or removed from the Browser.
  void WebStateAddedToBrowser(web::WebState* web_state);
  void WebStateRemovedFromBrowser(web::WebState* web_state);

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

  // OverlayRequestQueueImpl::Delegate:
  void OverlayRequestRemoved(OverlayRequestQueueImpl* queue,
                             std::unique_ptr<OverlayRequest> request,
                             bool cancelled) override;
  void OverlayRequestQueueWillReplaceDelegate(
      OverlayRequestQueueImpl* queue) override;

  // OverlayRequestQueueImpl::Observer:
  void RequestAddedToQueue(OverlayRequestQueueImpl* queue,
                           OverlayRequest* request,
                           size_t index) override;
  void OverlayRequestQueueDestroyed(OverlayRequestQueueImpl* queue) override;

  // OverlayPresentationContextObserver:
  void OverlayPresentationContextWillChangePresentationCapabilities(
      OverlayPresentationContext* presentation_context,
      OverlayPresentationContext::UIPresentationCapabilities capabilities)
      override;
  void OverlayPresentationContextDidChangePresentationCapabilities(
      OverlayPresentationContext* presentation_context) override;
  void OverlayPresentationContextDidEnableUI(
      OverlayPresentationContext* presentation_context) override;
  void OverlayPresentationContextDidMoveToWindow(
      OverlayPresentationContext* presentation_context,
      UIWindow* window) override;

  // WebStateListObserver:
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // Whether the UI delegate is presenting overlay UI for this presenter.  Stays
  // true from the beginning of the presentation until the end of the
  // dismissal.
  bool presenting_ = false;
  // Whether `detached_presenting_request_queue_` has replaced this
  // presenter as its delegate. This property will help manage a situation where
  // the WebState replaces another presenter with this presenter while an
  // overlay request is still presenting, requiring this presenter to cleanup
  // references to the request before the request is dismissed.
  bool detached_queue_replaced_delegate_ = false;
  // The OverlayRequestQueue owning `presented_request_` has recently been
  // detached.
  raw_ptr<OverlayRequestQueueImpl> detached_presenting_request_queue_ = nullptr;
  // The request whose overlay UI is currently being presented.  The value is
  // set when `presenting_` is set to true, and is reset to nullptr when
  // `presenting_` is reset to false.  May be different from GetActiveRequest()
  // if the front request of the active WebState's request queue is updated
  // while overlay UI is be presented.
  raw_ptr<OverlayRequest> presented_request_ = nullptr;
  // Whether the WebState that owns `presented_request_` is being detached.
  bool detaching_presenting_web_state_ = false;
  // Used to extend the lifetime of an OverlayRequest after being removed from
  // a queue until the completion of its dismissal flow.
  std::unique_ptr<OverlayRequest> removed_request_awaiting_dismissal_;
  // A set of all OverlayRequests that have been shown by the presenter.
  // Requests are removed when they are completed.
  std::set<OverlayRequest*> previously_presented_requests_;

  OverlayModality modality_;
  raw_ptr<WebStateList> web_state_list_ = nullptr;
  raw_ptr<web::WebState> active_web_state_ = nullptr;
  raw_ptr<OverlayPresentationContext> presentation_context_ = nullptr;
  base::ObserverList<OverlayPresenterObserver,
                     /* check_empty= */ true>
      observers_;
  // Scoped observation.
  base::ScopedObservation<Browser, BrowserObserver> browser_observation_{this};

  base::WeakPtrFactory<OverlayPresenterImpl> weak_factory_{this};
  // Add new members before weak_factory_.
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_PRESENTER_IMPL_H_
