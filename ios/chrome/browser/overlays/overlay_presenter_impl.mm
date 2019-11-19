// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_presenter_impl.h"

#include "base/logging.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    : modality_(modality),
      web_state_list_(browser->GetWebStateList()),
      weak_factory_(this) {
  browser->AddObserver(this);
  DCHECK(web_state_list_);
  web_state_list_->AddObserver(this);
  for (int i = 0; i < web_state_list_->count(); ++i) {
    GetQueueForWebState(web_state_list_->GetWebStateAt(i))->AddObserver(this);
  }
  SetActiveWebState(web_state_list_->GetActiveWebState(), CHANGE_REASON_NONE);
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

  // Reset |presenting| since it was tracking the status for the previous
  // delegate's presentation context.
  presenting_ = false;

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

void OverlayPresenterImpl::SetActiveWebState(
    web::WebState* web_state,
    WebStateListObserver::ChangeReason reason) {
  if (active_web_state_ == web_state)
    return;

  OverlayRequest* previously_active_request = GetActiveRequest();

  // The UI should be cancelled instead of hidden if the presenter does not
  // expect to show any more overlay UI for previously active WebState in the UI
  // delegate's presentation context.  This occurs:
  // - when the active WebState is replaced, and
  // - when the active WebState is detached from the WebStateList.
  bool should_cancel_ui =
      (reason & CHANGE_REASON_REPLACED) || detaching_active_web_state_;

  active_web_state_ = web_state;
  detaching_active_web_state_ = false;

  // Early return if there's no UI delegate, since presentation cannot occur.
  if (!presentation_context_)
    return;

  // If not already presenting, immediately show the next overlay.
  if (!presenting_) {
    PresentOverlayForActiveRequest();
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
    presentation_context_->HideOverlayUI(this, previously_active_request);
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
  // Overlays cannot be presented if one is already presented.
  DCHECK(!presenting_);

  // Overlays cannot be shown without a presentation context.
  if (!presentation_context_)
    return;

  // No presentation is necessary if there is no active reqeust or the context
  // is unable to show it.
  OverlayRequest* request = GetActiveRequest();
  if (!request || !presentation_context_->CanShowUIForRequest(request))
    return;

  presenting_ = true;

  // Notify the observers that the overlay UI is about to be shown.
  for (auto& observer : observers_) {
    observer.WillShowOverlay(this, request);
  }

  // Present the overlay UI via the UI delegate.
  OverlayPresentationCallback presentation_callback = base::BindOnce(
      &OverlayPresenterImpl::OverlayWasPresented, weak_factory_.GetWeakPtr(),
      presentation_context_, request);
  OverlayDismissalCallback dismissal_callback = base::BindOnce(
      &OverlayPresenterImpl::OverlayWasDismissed, weak_factory_.GetWeakPtr(),
      presentation_context_, request, GetActiveQueue()->GetWeakPtr());
  presentation_context_->ShowOverlayUI(this, request,
                                       std::move(presentation_callback),
                                       std::move(dismissal_callback));
}

void OverlayPresenterImpl::OverlayWasPresented(
    OverlayPresentationContext* presentation_context,
    OverlayRequest* request) {
  DCHECK_EQ(presentation_context_, presentation_context);
  DCHECK_EQ(GetActiveRequest(), request);
  for (auto& observer : observers_) {
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

  // Pop the request for overlays dismissed by the user.  The check against the
  // queue's front request prevents popping the request twice in the event that
  // the front request was cancelled by the queue during a user-triggered
  // dismissal.  |popped_request| is used to extend the lifetime of the request
  // past the DidHideOverlay() callbacks.
  std::unique_ptr<OverlayRequest> popped_request;
  if (reason == OverlayDismissalReason::kUserInteraction && queue &&
      queue->front_request() == request) {
    popped_request = queue->PopFrontRequest();
  }

  presenting_ = false;

  // Notify the observers that the overlay UI was hidden.
  for (auto& observer : observers_) {
    observer.DidHideOverlay(this, request);
  }

  // Only show the next overlay if the active request has changed, either
  // because the frontmost request was popped or because the active WebState has
  // changed.
  if (GetActiveRequest() != request)
    PresentOverlayForActiveRequest();
}

#pragma mark Cancellation helpers

void OverlayPresenterImpl::CancelOverlayUIForRequest(OverlayRequest* request) {
  if (!presentation_context_ || !request)
    return;
  presentation_context_->CancelOverlayUI(this, request);
}

void OverlayPresenterImpl::CancelAllOverlayUI() {
  for (int i = 0; i < web_state_list_->count(); ++i) {
    CancelOverlayUIForRequest(
        GetFrontRequestForWebState(web_state_list_->GetWebStateAt(i)));
  }
}

#pragma mark -
#pragma mark BrowserObserver

void OverlayPresenterImpl::BrowserDestroyed(Browser* browser) {
  SetPresentationContext(nullptr);
  SetActiveWebState(nullptr, CHANGE_REASON_NONE);

  for (int i = 0; i < web_state_list_->count(); ++i) {
    GetQueueForWebState(web_state_list_->GetWebStateAt(i))
        ->RemoveObserver(this);
  }
  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;
  browser->RemoveObserver(this);
}

#pragma mark OverlayRequestQueueImpl::Observer

void OverlayPresenterImpl::RequestAddedToQueue(OverlayRequestQueueImpl* queue,
                                               OverlayRequest* request) {
  // If |queue| is active, the added request is frontmost, and an overlay is not
  // currently being presented, trigger the UI presentation for that request.
  if (queue == GetActiveQueue() && request == queue->front_request() &&
      !presenting_) {
    PresentOverlayForActiveRequest();
  }
}

void OverlayPresenterImpl::QueuedRequestCancelled(
    OverlayRequestQueueImpl* queue,
    OverlayRequest* request) {
  CancelOverlayUIForRequest(request);
}

#pragma mark - OverlayPresentationContextObserver

void OverlayPresenterImpl::
    OverlayPresentationContextWillChangePresentationCapabilities(
        OverlayPresentationContext* presentation_context,
        OverlayPresentationContext::UIPresentationCapabilities capabilities) {
  DCHECK_EQ(presentation_context_, presentation_context);
  // Hide the presented overlay UI if the presentation context is transitioning
  // to a state where that UI is not supported.
  OverlayRequest* request = GetActiveRequest();
  if (presenting_ &&
      !presentation_context->CanShowUIForRequest(request, capabilities)) {
    presentation_context_->HideOverlayUI(this, GetActiveRequest());
  }
}

void OverlayPresenterImpl::
    OverlayPresentationContextDidChangePresentationCapabilities(
        OverlayPresentationContext* presentation_context) {
  DCHECK_EQ(presentation_context_, presentation_context);
  if (!presenting_)
    PresentOverlayForActiveRequest();
}

#pragma mark - WebStateListObserver

void OverlayPresenterImpl::WebStateInsertedAt(WebStateList* web_state_list,
                                              web::WebState* web_state,
                                              int index,
                                              bool activating) {
  GetQueueForWebState(web_state)->AddObserver(this);
}

void OverlayPresenterImpl::WebStateReplacedAt(WebStateList* web_state_list,
                                              web::WebState* old_web_state,
                                              web::WebState* new_web_state,
                                              int index) {
  GetQueueForWebState(old_web_state)->RemoveObserver(this);
  GetQueueForWebState(new_web_state)->AddObserver(this);
  if (old_web_state != active_web_state_) {
    // If the active WebState is being replaced, its overlay UI will be
    // cancelled later when |new_web_state| is activated.  For inactive WebState
    // replacements, the overlay UI can be cancelled immediately.
    CancelOverlayUIForRequest(GetFrontRequestForWebState(old_web_state));
  }
}

void OverlayPresenterImpl::WillDetachWebStateAt(WebStateList* web_state_list,
                                                web::WebState* web_state,
                                                int index) {
  GetQueueForWebState(web_state)->RemoveObserver(this);
  detaching_active_web_state_ = web_state == active_web_state_;
  if (!detaching_active_web_state_) {
    // If the active WebState is being detached, its overlay UI will be
    // cancelled later when the active WebState is reset.  For inactive WebState
    // replacements, the overlay UI can be cancelled immediately.
    CancelOverlayUIForRequest(GetFrontRequestForWebState(web_state));
  }
}

void OverlayPresenterImpl::WebStateActivatedAt(WebStateList* web_state_list,
                                               web::WebState* old_web_state,
                                               web::WebState* new_web_state,
                                               int active_index,
                                               int reason) {
  SetActiveWebState(new_web_state,
                    static_cast<WebStateListObserver::ChangeReason>(reason));
}
