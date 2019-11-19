// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_impl.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/callback.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presentation_context_observer.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - OverlayPresentationContextImpl::Container

OVERLAY_USER_DATA_SETUP_IMPL(OverlayPresentationContextImpl::Container);

OverlayPresentationContextImpl::Container::Container(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
}

OverlayPresentationContextImpl::Container::~Container() = default;

OverlayPresentationContextImpl*
OverlayPresentationContextImpl::Container::PresentationContextForModality(
    OverlayModality modality) {
  auto& ui_delegate = ui_delegates_[modality];
  if (!ui_delegate) {
    ui_delegate = base::WrapUnique(
        new OverlayPresentationContextImpl(browser_, modality));
  }
  return ui_delegate.get();
}

#pragma mark - OverlayPresentationContextImpl

OverlayPresentationContextImpl::OverlayPresentationContextImpl(
    Browser* browser,
    OverlayModality modality)
    : presenter_(OverlayPresenter::FromBrowser(browser, modality)),
      shutdown_helper_(browser, presenter_),
      coordinator_delegate_(this),
      coordinator_factory_([OverlayRequestCoordinatorFactory
          factoryForBrowser:browser
                   modality:modality]),
      weak_factory_(this) {
  DCHECK(presenter_);
  DCHECK(coordinator_factory_);
  presenter_->SetPresentationContext(this);
}

OverlayPresentationContextImpl::~OverlayPresentationContextImpl() = default;

#pragma mark Public

void OverlayPresentationContextImpl::SetCoordinator(
    OverlayContainerCoordinator* coordinator) {
  if (coordinator_ == coordinator)
    return;

  UpdateForCoordinator(coordinator);

  // The new coordinator should be started before provided to the UI delegate.
  DCHECK(!coordinator_ || coordinator_.viewController);
}

void OverlayPresentationContextImpl::WindowDidChange() {
  UpdateForCoordinator(coordinator_);
}

#pragma mark OverlayPresentationContext

void OverlayPresentationContextImpl::AddObserver(
    OverlayPresentationContextObserver* observer) {
  observers_.AddObserver(observer);
}

void OverlayPresentationContextImpl::RemoveObserver(
    OverlayPresentationContextObserver* observer) {
  observers_.RemoveObserver(observer);
}

OverlayPresentationContext::UIPresentationCapabilities
OverlayPresentationContextImpl::GetPresentationCapabilities() const {
  return presentation_capabilities_;
}

bool OverlayPresentationContextImpl::CanShowUIForRequest(
    OverlayRequest* request,
    UIPresentationCapabilities capabilities) const {
  BOOL uses_child_view_controller = [coordinator_factory_
      coordinatorForRequestUsesChildViewController:request];
  UIPresentationCapabilities required_capability =
      uses_child_view_controller ? UIPresentationCapabilities::kContained
                                 : UIPresentationCapabilities::kPresented;
  return !!(capabilities & required_capability);
}

bool OverlayPresentationContextImpl::CanShowUIForRequest(
    OverlayRequest* request) const {
  return CanShowUIForRequest(request, GetPresentationCapabilities());
}

void OverlayPresentationContextImpl::ShowOverlayUI(
    OverlayPresenter* presenter,
    OverlayRequest* request,
    OverlayPresentationCallback presentation_callback,
    OverlayDismissalCallback dismissal_callback) {
  DCHECK_EQ(presenter_, presenter);
  // Create the UI state for |request| if necessary.
  if (!GetRequestUIState(request))
    states_[request] = std::make_unique<OverlayRequestUIState>(request);
  // Present the overlay UI and update the UI state.
  GetRequestUIState(request)->OverlayPresentionRequested(
      std::move(presentation_callback), std::move(dismissal_callback));
  SetRequest(request);
}

void OverlayPresentationContextImpl::HideOverlayUI(OverlayPresenter* presenter,
                                                   OverlayRequest* request) {
  DCHECK_EQ(presenter_, presenter);
  DCHECK_EQ(request_, request);

  OverlayRequestUIState* state = GetRequestUIState(request_);
  DCHECK(state->has_callback());

  if (coordinator_) {
    // If the request's UI is presented by the coordinator, dismiss it.  The
    // presented request will be reset when the dismissal animation finishes.
    DismissPresentedUI(OverlayDismissalReason::kHiding);
  } else {
    // Simulate dismissal if there is no container coordinator.
    state->set_dismissal_reason(OverlayDismissalReason::kHiding);
    OverlayUIWasDismissed();
  }
}

void OverlayPresentationContextImpl::CancelOverlayUI(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  DCHECK_EQ(presenter_, presenter);

  // No cleanup required if there is no UI state for |request|.  This can
  // occur when cancelling an OverlayRequest whose UI has never been
  // presented.
  OverlayRequestUIState* state = GetRequestUIState(request);
  if (!state)
    return;

  // If the coordinator is not presenting the overlay UI for |state|, it can
  // be deleted immediately.
  if (!state->has_callback()) {
    states_.erase(request);
    return;
  }

  // If the current request is being cancelled (e.g. for WebState closures) when
  // there is no coordinator, simulate a dismissal.
  if (!coordinator_) {
    DCHECK_EQ(request_, request);
    state->set_dismissal_reason(OverlayDismissalReason::kCancellation);
    state->OverlayUIWasDismissed();
    return;
  }

  DismissPresentedUI(OverlayDismissalReason::kCancellation);
}

#pragma mark Accesors

void OverlayPresentationContextImpl::SetRequest(OverlayRequest* request) {
  if (request_ == request)
    return;
  if (request_) {
    OverlayRequestUIState* state = GetRequestUIState(request_);
    // The presented request should only be reset when the previously presented
    // request's UI has finished being dismissed.
    DCHECK(state);
    DCHECK(!state->has_callback());
    DCHECK(!state->coordinator().viewController.view.superview);
    // If the overlay was dismissed for user interaction or cancellation, then
    // the state can be destroyed, since the UI for the previously presented
    // request will never be shown again.
    OverlayDismissalReason reason = state->dismissal_reason();
    if (reason == OverlayDismissalReason::kUserInteraction ||
        reason == OverlayDismissalReason::kCancellation) {
      states_.erase(request_);
    }
  }

  request_ = request;

  // The UI state should be created before resetting the presented request.
  DCHECK(!request_ || GetRequestUIState(request_));

  ShowUIForPresentedRequest();
}

OverlayRequestUIState* OverlayPresentationContextImpl::GetRequestUIState(
    OverlayRequest* request) {
  return request ? states_[request].get() : nullptr;
}

void OverlayPresentationContextImpl::UpdateForCoordinator(
    OverlayContainerCoordinator* coordinator) {
  UIPresentationCapabilities capabilities = UIPresentationCapabilities::kNone;
  UIViewController* view_controller = coordinator.viewController;
  // Any UIViewController can contain overlay UI as a child.
  if (view_controller) {
    capabilities = static_cast<UIPresentationCapabilities>(
        capabilities | UIPresentationCapabilities::kContained);
  }
  // Only UIViewControllers attached to a window can present overlay UI.
  if (view_controller.view.window) {
    capabilities = static_cast<UIPresentationCapabilities>(
        capabilities | UIPresentationCapabilities::kPresented);
  }
  bool capabilities_changed = presentation_capabilities_ != capabilities;

  if (capabilities_changed) {
    for (auto& observer : observers_) {
      observer.OverlayPresentationContextWillChangePresentationCapabilities(
          this, capabilities);
    }
  }

  presentation_capabilities_ = capabilities;
  coordinator_ = coordinator;

  if (capabilities_changed) {
    for (auto& observer : observers_) {
      observer.OverlayPresentationContextDidChangePresentationCapabilities(
          this);
    }
  }
}

#pragma mark Presentation and Dismissal helpers

void OverlayPresentationContextImpl::ShowUIForPresentedRequest() {
  OverlayRequestUIState* state = GetRequestUIState(request_);
  if (!state || !coordinator_)
    return;

  // Create the coordinator if necessary.
  UIViewController* container_view_controller = coordinator_.viewController;
  OverlayRequestCoordinator* overlay_coordinator = state->coordinator();
  if (!overlay_coordinator ||
      overlay_coordinator.baseViewController != container_view_controller) {
    overlay_coordinator = [coordinator_factory_
        newCoordinatorForRequest:request_
                        delegate:&coordinator_delegate_
              baseViewController:container_view_controller];
    state->OverlayUIWillBePresented(overlay_coordinator);
  }

  [overlay_coordinator startAnimated:!state->has_ui_been_presented()];
  state->OverlayUIWasPresented();
}

void OverlayPresentationContextImpl::OverlayUIWasPresented() {
  OverlayRequestUIState* state = GetRequestUIState(request_);
  DCHECK(state);
  UIView* overlay_view = state->coordinator().viewController.view;
  DCHECK(overlay_view);
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  overlay_view);
}

void OverlayPresentationContextImpl::DismissPresentedUI(
    OverlayDismissalReason reason) {
  OverlayRequestUIState* state = GetRequestUIState(request_);
  DCHECK(state);
  DCHECK(coordinator_);
  DCHECK(state->coordinator());

  state->set_dismissal_reason(reason);
  [state->coordinator()
      stopAnimated:reason == OverlayDismissalReason::kUserInteraction];
}

void OverlayPresentationContextImpl::OverlayUIWasDismissed() {
  DCHECK(request_);
  DCHECK(GetRequestUIState(request_)->has_callback());
  // If there is another request in the active WebState's OverlayRequestQueue,
  // executing the state's dismissal callback will trigger the presentation of
  // the next request.  If the presented request remains unchanged after calling
  // the dismissal callback, reset it to nullptr since the UI is no longer
  // presented.
  OverlayRequest* previously_presented_request = request_;
  GetRequestUIState(request_)->OverlayUIWasDismissed();
  if (request_ == previously_presented_request)
    SetRequest(nullptr);
}

#pragma mark BrowserShutdownHelper

OverlayPresentationContextImpl::BrowserShutdownHelper::BrowserShutdownHelper(
    Browser* browser,
    OverlayPresenter* presenter)
    : presenter_(presenter) {
  DCHECK(presenter_);
  browser->AddObserver(this);
}

OverlayPresentationContextImpl::BrowserShutdownHelper::
    ~BrowserShutdownHelper() = default;

void OverlayPresentationContextImpl::BrowserShutdownHelper::BrowserDestroyed(
    Browser* browser) {
  presenter_->SetPresentationContext(nullptr);
  browser->RemoveObserver(this);
}

#pragma mark OverlayDismissalHelper

OverlayPresentationContextImpl::OverlayRequestCoordinatorDelegateImpl::
    OverlayRequestCoordinatorDelegateImpl(
        OverlayPresentationContextImpl* presentation_context)
    : presentation_context_(presentation_context) {
  DCHECK(presentation_context_);
}

OverlayPresentationContextImpl::OverlayRequestCoordinatorDelegateImpl::
    ~OverlayRequestCoordinatorDelegateImpl() = default;

void OverlayPresentationContextImpl::OverlayRequestCoordinatorDelegateImpl::
    OverlayUIDidFinishPresentation(OverlayRequest* request) {
  DCHECK(request);
  DCHECK_EQ(presentation_context_->request_, request);
  presentation_context_->OverlayUIWasPresented();
}

void OverlayPresentationContextImpl::OverlayRequestCoordinatorDelegateImpl::
    OverlayUIDidFinishDismissal(OverlayRequest* request) {
  DCHECK(request);
  DCHECK_EQ(presentation_context_->request_, request);
  presentation_context_->OverlayUIWasDismissed();
}
