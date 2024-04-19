// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/overlay_presenter_impl.h"

#import "base/check_op.h"
#import "base/containers/contains.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#pragma mark - Factory method

// static
OverlayPresenter* OverlayPresenter::FromBrowser(Browser* browser,
                                                OverlayModality modality) {
  OverlayPresenterImpl::Container::CreateForUserData(browser, browser);
  return OverlayPresenterImpl::Container::FromUserData(browser)
      ->PresenterForModality(modality);
}

#pragma mark - OverlayPresenterImpl::Container

OVERLAY_USER_DATA_SETUP_IMPL(OverlayPresenterImpl::Container);

OverlayPresenterImpl::Container::Container(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
}

OverlayPresenterImpl::Container::~Container() = default;

OverlayPresenterImpl* OverlayPresenterImpl::Container::PresenterForModality(
    OverlayModality modality) {
  auto& presenter = presenters_[modality];
  if (!presenter) {
    presenter = base::WrapUnique(new OverlayPresenterImpl(browser_, modality));
  }
  return presenter.get();
}

#pragma mark - OverlayPresenterImpl

OverlayPresenterImpl::OverlayPresenterImpl(Browser* browser,
                                           OverlayModality modality)
    : modality_(modality), web_state_list_(browser->GetWebStateList()) {
  browser_observation_.Observe(browser);
  DCHECK(web_state_list_);
  web_state_list_->AddObserver(this);
  for (int i = 0; i < web_state_list_->count(); ++i) {
    WebStateAddedToBrowser(web_state_list_->GetWebStateAt(i));
  }
  SetActiveWebState(web_state_list_->GetActiveWebState(),
                    /*is_replaced=*/false);
}

OverlayPresenterImpl::~OverlayPresenterImpl() {
  // The presenter should be disconnected from WebStateList changes before
  // destruction.
  DCHECK(!presentation_context_);
  DCHECK(!web_state_list_);

  for (auto& observer : observers_) {
    observer.OverlayPresenterDestroyed(this);
  }
}

#pragma mark - Public

#pragma mark OverlayPresenter

OverlayModality OverlayPresenterImpl::GetModality() const {
  return modality_;
}

void OverlayPresenterImpl::SetPresentationContext(
    OverlayPresentationContext* presentation_context) {
  // When the presentation context is reset, the presenter will begin showing
  // overlays in the new presentation context.  Cancel overlay state from the
  // previous context since this Browser's overlays will no longer be presented
  // there.
  if (presentation_context_) {
    CancelAllOverlayUI();
    presentation_context_->RemoveObserver(this);
  }

  presentation_context_ = presentation_context;

  // Reset `presenting` since it was tracking the status for the previous
  // delegate's presentation context.
  presenting_ = false;
  presented_request_ = nullptr;
  previously_presented_requests_.clear();

  if (presentation_context_) {
    presentation_context_->AddObserver(this);
    PresentOverlayForActiveRequest();
  }
}

void OverlayPresenterImpl::AddObserver(OverlayPresenterObserver* observer) {
  observers_.AddObserver(observer);
}

void OverlayPresenterImpl::RemoveObserver(OverlayPresenterObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool OverlayPresenterImpl::IsShowingOverlayUI() const {
  return presenting_;
}

#pragma mark - Private

#pragma mark Accessors

void OverlayPresenterImpl::SetActiveWebState(web::WebState* web_state,
                                             bool is_replaced) {
  if (active_web_state_ == web_state) {
    return;
  }

  OverlayRequest* previously_active_request =
      removed_request_awaiting_dismissal_ != nullptr
          ? removed_request_awaiting_dismissal_.get()
          : GetActiveRequest();

  // The UI should be cancelled instead of hidden if the presenter does not
  // expect to show any more overlay UI for previously active WebState in the UI
  // delegate's presentation context.  This occurs:
  // - when the presenting WebState is replaced, and
  // - when the presenting WebState is detached from the WebStateList.
  const bool should_cancel_ui = is_replaced || detaching_presenting_web_state_;

  active_web_state_ = web_state;
  detaching_presenting_web_state_ = false;

  // Early return if there's no UI delegate, since presentation cannot occur.
  if (!presentation_context_) {
    return;
  }

  // If not already presenting, immediately show the next overlay.
  if (!presenting_) {
    PresentOverlayForActiveRequest();
    return;
  }

  // If presenting_ is true and there is no previously active request, this
  // is likely because the presenting overlay is still in the process of being
  // dismissed and multiple tabs have been opened in the process.
  if (!previously_active_request) {
    return;
  }

  // If the active WebState changes while an overlay is being presented, the
  // presented UI needs to be dismissed before the next overlay for the new
  // active WebState can be shown.  The new active WebState's overlays will be
  // presented when the previous overlay's dismissal callback is executed.
  DCHECK(previously_active_request);
  if (should_cancel_ui) {
    CancelOverlayUIForRequest(previously_active_request);
  } else {
    // For WebState activations, the overlay UI for the previously active
    // WebState should be hidden, as it may be shown again upon reactivating.
    presentation_context_->HideOverlayUI(previously_active_request);
  }
}

OverlayRequestQueueImpl* OverlayPresenterImpl::GetQueueForWebState(
    web::WebState* web_state) const {
  if (!web_state)
    return nullptr;
  OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
  return OverlayRequestQueueImpl::Container::FromWebState(web_state)
      ->QueueForModality(modality_);
}

OverlayRequest* OverlayPresenterImpl::GetFrontRequestForWebState(
    web::WebState* web_state) const {
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
  return queue ? queue->front_request() : nullptr;
}

OverlayRequestQueueImpl* OverlayPresenterImpl::GetActiveQueue() const {
  return GetQueueForWebState(active_web_state_);
}

OverlayRequest* OverlayPresenterImpl::GetActiveRequest() const {
  return GetFrontRequestForWebState(active_web_state_);
}

#pragma mark UI Presentation and Dismissal helpers

void OverlayPresenterImpl::PresentOverlayForActiveRequest() {

  // Overlays cannot be shown without a presentation context or if the
  // presentation context is already showing overlay UI.
  if (!presentation_context_ || presentation_context_->IsShowingOverlayUI())
    return;

  // No presentation is necessary if there is no active reqeust.
  OverlayRequest* request = GetActiveRequest();
  if (!request)
    return;

  // If the UI is disabled, no presentation nor preparation should occur.
  // `PrepareToShowOverlayUI()` is dismissing the keyboard, so do an early
  // return.
  if (presentation_context_->IsUIDisabled()) {
    return;
  }

  // Presentation cannot occur if the context is currently unable to show the UI
  // for `request`.  Attempt to prepare the presentation context for `request`.
  if (!presentation_context_->CanShowUIForRequest(request)) {
    presentation_context_->PrepareToShowOverlayUI(request);
    return;
  }

  // If an overlay is already presented, the Presentation Context should be
  // marked as showing an Overlay.
  DCHECK(!presenting_);
  presenting_ = true;
  presented_request_ = request;

  // Notify the observers that the overlay UI is about to be shown.
  bool initial_presentation =
      !base::Contains(previously_presented_requests_, request);
  for (auto& observer : observers_) {
    if (observer.GetRequestSupport(this)->IsRequestSupported(request))
      observer.WillShowOverlay(this, request, initial_presentation);
  }

  // Record that the request was shown, and add the completion callback to
  // remove the request from the set.
  previously_presented_requests_.insert(request);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&OverlayPresenterImpl::OverlayWasCompleted,
                     weak_factory_.GetWeakPtr(), request));

  // Present the overlay UI via the UI delegate.
  OverlayPresentationCallback presentation_callback = base::BindOnce(
      &OverlayPresenterImpl::OverlayWasPresented, weak_factory_.GetWeakPtr(),
      presentation_context_, request);
  OverlayDismissalCallback dismissal_callback = base::BindOnce(
      &OverlayPresenterImpl::OverlayWasDismissed, weak_factory_.GetWeakPtr(),
      // TODO(crbug.com/40061562): Remove `UnsafeDanglingUntriaged`
      presentation_context_, base::UnsafeDanglingUntriaged(request),
      GetActiveQueue()->GetWeakPtr());
  presentation_context_->ShowOverlayUI(
      request, std::move(presentation_callback), std::move(dismissal_callback));
}

void OverlayPresenterImpl::OverlayWasPresented(
    OverlayPresentationContext* presentation_context,
    OverlayRequest* request) {
  DCHECK_EQ(presentation_context_, presentation_context);
  DCHECK_EQ(presented_request_, request);
  for (auto& observer : observers_) {
    if (observer.GetRequestSupport(this)->IsRequestSupported(request))
      observer.DidShowOverlay(this, request);
  }
}

void OverlayPresenterImpl::OverlayWasDismissed(
    OverlayPresentationContext* presentation_context,
    OverlayRequest* request,
    base::WeakPtr<OverlayRequestQueueImpl> queue,
    OverlayDismissalReason reason) {
  // If the UI delegate is reset while presenting an overlay, that overlay will
  // be cancelled and dismissed.  The presenter is now using the new UI
  // delegate's presentation context, so this dismissal should not trigger
  // presentation logic.
  if (presentation_context_ != presentation_context)
    return;

  // When the presenter has been replaced as the delegate of the active
  // OverlayRequestQueue, observers are notified of DidHideOverlay() and
  // `presented_request_` is reset early. Thus, there is no need to do any
  // dismissal bookkeeping since the request has been removed.
  if (detached_queue_replaced_delegate_) {
    presenting_ = false;
    detached_queue_replaced_delegate_ = false;
    if (GetActiveRequest()) {
      PresentOverlayForActiveRequest();
    }
    return;
  }

  DCHECK_EQ(presented_request_, request);

  // Pop the request for overlays dismissed by the user.  The check against
  // `removed_request_awaiting_dismissal_` prevents the queue's front request
  // from being popped if this dismissal was caused by `request`'s removal from
  // the queue.
  if (reason == OverlayDismissalReason::kUserInteraction && queue &&
      request != removed_request_awaiting_dismissal_.get()) {
    queue->PopFrontRequest();
    // Popping the request should transfer ownership of the request to the
    // OverlayPresenter until the completion of DidHideOverlay() observer
    // callbacks below.
    DCHECK_EQ(removed_request_awaiting_dismissal_.get(), request);
  }

  presenting_ = false;
  presented_request_ = nullptr;
  // The OverlayPresenter remains as the delegate for
  // `detached_presenting_request_queue_` to ensure that `presented_request_` is
  // not deleted before the dismissal of its UI is finished.  Since the UI is
  // now being dismissed, this reference is not needed anymore.
  detached_presenting_request_queue_ = nullptr;

  // Notify the observers that the overlay UI was hidden.
  for (auto& observer : observers_) {
    if (observer.GetRequestSupport(this)->IsRequestSupported(request))
      observer.DidHideOverlay(this, request);
  }

  // Now that observers have been notified that the UI for `request` was hidden,
  // `removed_request_awaiting_dismissal_` can be reset since the request no
  // longer needs to be kept alive.
  removed_request_awaiting_dismissal_ = nullptr;

  // Only show the next overlay if the active request has changed, either
  // because the frontmost request was popped or because the active WebState has
  // changed.
  if (GetActiveRequest() != request)
    PresentOverlayForActiveRequest();
}

void OverlayPresenterImpl::OverlayWasCompleted(OverlayRequest* request,
                                               OverlayResponse* response) {
  previously_presented_requests_.erase(request);
}

#pragma mark UI Cancellation helpers

void OverlayPresenterImpl::CancelOverlayUIForRequest(OverlayRequest* request) {
  if (!presentation_context_ || !request)
    return;
  presentation_context_->CancelOverlayUI(request);
}

void OverlayPresenterImpl::CancelAllOverlayUI() {
  for (int i = 0; i < web_state_list_->count(); ++i) {
    CancelOverlayUIForRequest(
        GetFrontRequestForWebState(web_state_list_->GetWebStateAt(i)));
  }
}

#pragma mark WebState helpers

void OverlayPresenterImpl::WebStateAddedToBrowser(web::WebState* web_state) {
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
  queue->AddObserver(this);
  queue->SetDelegate(this);
}

void OverlayPresenterImpl::WebStateRemovedFromBrowser(
    web::WebState* web_state) {
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
  queue->RemoveObserver(this);
  // Only reset the delegate if there isn't a currently presented overlay or
  // `presented_request_`'s WebState is not the WebState being removed. This
  // will allow the presenter to extend the lifetime of `presented_request_` if
  // it is removed from the queue before its dismissal finishes.
  if (!presented_request_ ||
      presented_request_->GetQueueWebState() != web_state) {
    queue->SetDelegate(nullptr);
  }

  if (presented_request_ &&
      presented_request_->GetQueueWebState() == web_state) {
    detached_presenting_request_queue_ = GetQueueForWebState(web_state);
  } else {
    // For inactive WebState removals, the overlay UI can be cancelled
    // immediately.
    CancelOverlayUIForRequest(GetFrontRequestForWebState(web_state));
  }
}

#pragma mark -
#pragma mark BrowserObserver

void OverlayPresenterImpl::BrowserDestroyed(Browser* browser) {
  SetPresentationContext(nullptr);
  SetActiveWebState(nullptr, /*is_replaced=*/false);

  for (int i = 0; i < web_state_list_->count(); ++i) {
    WebStateRemovedFromBrowser(web_state_list_->GetWebStateAt(i));
  }
  // All Webstates are detached before the Browser is destroyed so all request
  // must be cancelled at this point.
  DCHECK(!detached_presenting_request_queue_);
  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;
  removed_request_awaiting_dismissal_ = nullptr;
  browser_observation_.Reset();
}

#pragma mark OverlayRequestQueueImpl::Delegate

void OverlayPresenterImpl::OverlayRequestRemoved(
    OverlayRequestQueueImpl* queue,
    std::unique_ptr<OverlayRequest> request,
    bool cancelled) {
  OverlayRequest* removed_request = request.get();
  if (presented_request_ == removed_request) {
    removed_request_awaiting_dismissal_ = std::move(request);
    if (detached_presenting_request_queue_) {
      detached_presenting_request_queue_ = nullptr;
      queue->SetDelegate(nullptr);
    }
  }
  if (cancelled)
    CancelOverlayUIForRequest(removed_request);
}

void OverlayPresenterImpl::OverlayRequestQueueWillReplaceDelegate(
    OverlayRequestQueueImpl* queue) {
  if (!presented_request_ || presented_request_ != queue->front_request())
    return;
  // If `presented_request_` is in the queue that is replacing this presenter
  // as the delegate, it is no longer possible to extend the lifetime of
  // `presented_request_`. Thus, call DidHideOverlay while it is still valid
  // and reset its reference.
  for (auto& observer : observers_) {
    if (observer.GetRequestSupport(this)->IsRequestSupported(
            presented_request_)) {
      observer.DidHideOverlay(this, presented_request_);
    }
  }
  presented_request_ = nullptr;
  detached_presenting_request_queue_ = nullptr;
  detached_queue_replaced_delegate_ = true;
}

#pragma mark OverlayRequestQueueImpl::Observer

void OverlayPresenterImpl::RequestAddedToQueue(OverlayRequestQueueImpl* queue,
                                               OverlayRequest* request,
                                               size_t index) {
  // If `request` is not active, there is no need to trigger any presentation.
  if (request != GetActiveRequest())
    return;

  // If the added request is active and there is no presentation occurring,
  // present the overlay UI immediately.
  if (!presenting_) {
    PresentOverlayForActiveRequest();
    return;
  }

  // `request` is the new active request, but overlay UI is already
  // presented.  This occurs when:
  // 1. `request` is added after `presented_request_` is cancelled, but
  //    before its UI is finished being dismissed,
  // 2. `request` is added immediately after a WebState activation, but
  //    before the overlay UI from the previously active WebState's front
  //    request is finished being dismissed, or
  // 3. `request` is inserted to the front of the active WebState's request
  //    queue.
  //
  // For scenarios (1) and (2), the UI is already in the process of being
  // dismissed, and `request`'s UI will be presented when that dismissal
  // finishes.  For scenario (3), the UI for the presented request needs to
  // be hidden so that the UI for `request` can be presented.
  bool should_dismiss_for_inserted_request =
      presented_request_ && queue->size() > 1 &&
      queue->GetRequest(/*index=*/1) == presented_request_;
  if (should_dismiss_for_inserted_request)
    presentation_context_->HideOverlayUI(presented_request_);
}

void OverlayPresenterImpl::OverlayRequestQueueDestroyed(
    OverlayRequestQueueImpl* queue) {
  queue->RemoveObserver(this);
}

#pragma mark - OverlayPresentationContextObserver

void OverlayPresenterImpl::
    OverlayPresentationContextWillChangePresentationCapabilities(
        OverlayPresentationContext* presentation_context,
        OverlayPresentationContext::UIPresentationCapabilities capabilities) {
  DCHECK_EQ(presentation_context_, presentation_context);
  // Hide the presented overlay UI if the presentation context is transitioning
  // to a state where that UI is not supported.
  if (presented_request_ && !presentation_context->CanShowUIForRequest(
                                presented_request_, capabilities)) {
    DCHECK(presenting_);
    presentation_context_->HideOverlayUI(presented_request_);
  }
}

void OverlayPresenterImpl::
    OverlayPresentationContextDidChangePresentationCapabilities(
        OverlayPresentationContext* presentation_context) {
  DCHECK_EQ(presentation_context_, presentation_context);
  if (!presenting_)
    PresentOverlayForActiveRequest();
}

void OverlayPresenterImpl::OverlayPresentationContextDidEnableUI(
    OverlayPresentationContext* presentation_context) {
  DCHECK_EQ(presentation_context_, presentation_context);
  if (!presenting_) {
    PresentOverlayForActiveRequest();
  }
}

void OverlayPresenterImpl::OverlayPresentationContextDidMoveToWindow(
    OverlayPresentationContext* presentation_context,
    UIWindow* window) {
  DCHECK_EQ(presentation_context_, presentation_context);
  if (!presenting_ && window)
    PresentOverlayForActiveRequest();
}

#pragma mark - WebStateListObserver

void OverlayPresenterImpl::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  web::WebState* detached_web_state = detach_change.detached_web_state();
  detaching_presenting_web_state_ =
      presented_request_
          ? presented_request_->GetQueueWebState() == detached_web_state
          : false;
  WebStateRemovedFromBrowser(detached_web_state);
}

void OverlayPresenterImpl::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // The activation is handled after this switch statement.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      WebStateRemovedFromBrowser(replace_change.replaced_web_state());
      WebStateAddedToBrowser(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      WebStateAddedToBrowser(insert_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }

  if (status.active_web_state_change()) {
    SetActiveWebState(status.new_active_web_state,
                      change.type() == WebStateListChange::Type::kReplace);
  }
}
