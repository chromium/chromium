// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl.h"

#import <UIKit/UIKit.h>

#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context_observer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_coordinator_factory.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_coordinator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl_delegate.h"

namespace {

// Returns the OverlayRequestId for `request` correctly handling null.
OverlayRequestId OverlayRequestIdForRequest(OverlayRequest* request) {
  if (!request) {
    return {};
  }

  return request->GetRequestId();
}

}  // namespace

// static
OverlayPresentationContextImpl* OverlayPresentationContextImpl::FromBrowser(
    Browser* browser,
    OverlayModality modality) {
  OverlayPresentationContextImpl::Container::CreateForUserData(browser,
                                                               browser);
  return OverlayPresentationContextImpl::Container::FromUserData(browser)
      ->PresentationContextForModality(modality);
}

// static
OverlayPresentationContext* OverlayPresentationContext::FromBrowser(
    Browser* browser,
    OverlayModality modality) {
  return OverlayPresentationContextImpl::FromBrowser(browser, modality);
}

#pragma mark - OverlayPresentationContextImpl::Container

OverlayPresentationContextImpl::Container::Container(Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
}

OverlayPresentationContextImpl::Container::~Container() = default;

OverlayPresentationContextImpl*
OverlayPresentationContextImpl::Container::PresentationContextForModality(
    OverlayModality modality) {
  // Use TestOverlayPresentationContext to create presentation contexts for
  // OverlayModality::kTesting.
  // TODO(crbug.com/40120484): Remove requirement once modalities are converted
  // to no longer use enums.
  DCHECK_NE(modality, OverlayModality::kTesting);

  auto& ui_delegate = ui_delegates_[modality];
  if (!ui_delegate) {
    OverlayRequestCoordinatorFactory* factory =
        [[OverlayRequestCoordinatorFactory alloc] initWithBrowser:browser_
                                                         modality:modality];
    ui_delegate = base::WrapUnique(
        new OverlayPresentationContextImpl(browser_, modality, factory));
  }
  return ui_delegate.get();
}

#pragma mark - OverlayPresentationContextImpl

OverlayPresentationContextImpl::OverlayPresentationContextImpl(
    Browser* browser,
    OverlayModality modality,
    OverlayRequestCoordinatorFactory* factory)
    : presenter_(OverlayPresenter::FromBrowser(browser, modality)),
      coordinator_delegate_(this),
      fullscreen_disabler_(browser, modality),
      coordinator_factory_(factory),
      weak_factory_(this) {
  DCHECK(presenter_);
  DCHECK(coordinator_factory_);
  presenter_->AddObserver(this);
  presenter_->SetPresentationContext(this);
}

OverlayPresentationContextImpl::~OverlayPresentationContextImpl() {
  if (presenter_) {
    // Destruction order between the OverlayPresentationContextImpl and the
    // OverlayPresenter is unspecified, so reset the PresentationContext if
    // the OverlayPresentationContextImpl is destroyed first.
    OverlayPresenterDestroyed(presenter_);
    DCHECK(!presenter_);
  }

  for (auto& [_, ui_state] : states_) {
    ui_state->coordinator().delegate = nil;
  }
}

#pragma mark Public

void OverlayPresentationContextImpl::SetDelegate(
    id<OverlayPresentationContextImplDelegate> delegate) {
  if (delegate_ == delegate) {
    return;
  }
  // Reset the presentation capabilities.
  container_view_controller_ = nil;
  presentation_context_view_controller_ = nil;
  UpdatePresentationCapabilities();

  delegate_ = delegate;

  // The context is only capable of presenting once the delegate is provided.
  presenter_->SetPresentationContext(delegate_ ? this : nullptr);
}

void OverlayPresentationContextImpl::SetWindow(UIWindow* window) {
  if (window_ == window) {
    return;
  }
  window_ = window;
  for (auto& observer : observers_) {
    observer.OverlayPresentationContextDidMoveToWindow(this, window_);
  }
}

void OverlayPresentationContextImpl::SetContainerViewController(
    UIViewController* view_controller) {
  if (container_view_controller_ == view_controller) {
    return;
  }
  container_view_controller_ = view_controller;
  UpdatePresentationCapabilities();
}

void OverlayPresentationContextImpl::SetPresentationContextViewController(
    UIViewController* view_controller) {
  if (presentation_context_view_controller_ == view_controller) {
    return;
  }
  presentation_context_view_controller_ = view_controller;
  // `view_controller` should not be provided to the context until it is fully
  // presented in a window.
  DCHECK(!view_controller ||
         (view_controller.presentationController.containerView.window &&
          !view_controller.beingPresented && !view_controller.beingDismissed));
  UpdatePresentationCapabilities();
}

void OverlayPresentationContextImpl::SetUIDisabled(bool disabled) {
  if (ui_disabled_ == disabled) {
    return;
  }
  ui_disabled_ = disabled;
  UpdatePresentationCapabilities();

  if (!disabled) {
    for (auto& observer : observers_) {
      observer.OverlayPresentationContextDidEnableUI(this);
    }
  }
}

bool OverlayPresentationContextImpl::IsUIDisabled() {
  return ui_disabled_;
}

#pragma mark OverlayPresenterObserver

void OverlayPresentationContextImpl::OverlayPresenterDestroyed(
    OverlayPresenter* presenter) {
  DCHECK_EQ(presenter_, presenter);
  presenter_->SetPresentationContext(nullptr);
  presenter_->RemoveObserver(this);
  presenter_ = nullptr;
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
  UIPresentationCapabilities required_capability =
      GetRequiredPresentationCapabilities(request);
  return !!(capabilities & required_capability);
}

bool OverlayPresentationContextImpl::CanShowUIForRequest(
    OverlayRequest* request) const {
  return CanShowUIForRequest(request, GetPresentationCapabilities());
}

bool OverlayPresentationContextImpl::IsShowingOverlayUI() const {
  // The UI for the active request is visible until its dismissal callback has
  // been executed.
  OverlayRequestUIState* state = GetRequestUIState(request_id_);
  return state && state->has_callback();
}

void OverlayPresentationContextImpl::PrepareToShowOverlayUI(
    OverlayRequest* request) {
  // Early return if the request is already supported.
  if (CanShowUIForRequest(request)) {
    return;
  }

  // Request the delegate to prepare for overlay UI with `required_capability`.
  UIPresentationCapabilities required_capabilities =
      GetRequiredPresentationCapabilities(request);
  [delegate_ updatePresentationContext:this
           forPresentationCapabilities:required_capabilities];
}

void OverlayPresentationContextImpl::ShowOverlayUI(
    OverlayRequest* request,
    OverlayPresentationCallback presentation_callback,
    OverlayDismissalCallback dismissal_callback) {
  DCHECK(!IsShowingOverlayUI());
  DCHECK(CanShowUIForRequest(request));
  // Create the UI state for `request` if necessary.
  const OverlayRequestId request_id = OverlayRequestIdForRequest(request);
  if (!GetRequestUIState(request_id)) {
    states_[request_id] = std::make_unique<OverlayRequestUIState>(request);
  }
  // Present the overlay UI and update the UI state.
  GetRequestUIState(request_id)
      ->OverlayPresentionRequested(std::move(presentation_callback),
                                   std::move(dismissal_callback));
  SetRequest(request);
}

void OverlayPresentationContextImpl::HideOverlayUI(OverlayRequest* request) {
  const OverlayRequestId request_id = OverlayRequestIdForRequest(request);
  DCHECK_EQ(request_id_, request_id);

  OverlayRequestUIState* state = GetRequestUIState(request_id);
  DCHECK(state->has_callback());

  // Hide the overlay UI.  The presented request will be reset when the
  // dismissal animation finishes.
  DismissPresentedUI(OverlayDismissalReason::kHiding);
}

void OverlayPresentationContextImpl::CancelOverlayUI(OverlayRequest* request) {
  // No cleanup required if there is no UI state for `request`.  This can
  // occur when cancelling an OverlayRequest whose UI has never been
  // presented.
  const OverlayRequestId request_id = OverlayRequestIdForRequest(request);
  OverlayRequestUIState* state = GetRequestUIState(request_id);
  if (!state) {
    return;
  }

  // If the coordinator is not presenting the overlay UI for `state`, it can
  // be deleted immediately.
  if (!state->has_callback()) {
    states_.erase(request_id);
    return;
  }

  DismissPresentedUI(OverlayDismissalReason::kCancellation);
}

#pragma mark Accesors

void OverlayPresentationContextImpl::SetRequest(OverlayRequest* request) {
  const OverlayRequestId request_id = OverlayRequestIdForRequest(request);
  if (request_id_ == request_id) {
    return;
  }
  if (request_id_) {
    OverlayRequestUIState* state = GetRequestUIState(request_id_);
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
      states_.erase(request_id_);
    }
  }

  request_id_ = request_id;

  if (request_id_) {
    // The UI state should be created before resetting the presented request.
    DCHECK(GetRequestUIState(request_id_));
    ShowUIForPresentedRequest(request);
  } else {
    // Inform the delegate that no presentation capabilities are currently
    // required.
    [delegate_ updatePresentationContext:this
             forPresentationCapabilities:UIPresentationCapabilities::kNone];
  }
}

bool OverlayPresentationContextImpl::RequestUsesChildViewController(
    OverlayRequest* request) const {
  return [coordinator_factory_
      coordinatorForRequestUsesChildViewController:request];
}

UIViewController* OverlayPresentationContextImpl::GetBaseViewController(
    OverlayRequest* request) const {
  return RequestUsesChildViewController(request)
             ? container_view_controller_
             : presentation_context_view_controller_;
}

OverlayRequestUIState* OverlayPresentationContextImpl::GetRequestUIState(
    OverlayRequestId request_id) const {
  auto iter = states_.find(request_id);
  return iter == states_.end() ? nullptr : iter->second.get();
}

OverlayPresentationContext::UIPresentationCapabilities
OverlayPresentationContextImpl::GetRequiredPresentationCapabilities(
    OverlayRequest* request) const {
  BOOL uses_child_view_controller = [coordinator_factory_
      coordinatorForRequestUsesChildViewController:request];
  return uses_child_view_controller ? UIPresentationCapabilities::kContained
                                    : UIPresentationCapabilities::kPresented;
}

void OverlayPresentationContextImpl::UpdatePresentationCapabilities() {
  UIPresentationCapabilities capabilities = ConstructPresentationCapabilities();
  bool capabilities_changed = presentation_capabilities_ != capabilities;

  if (capabilities_changed) {
    for (auto& observer : observers_) {
      observer.OverlayPresentationContextWillChangePresentationCapabilities(
          this, capabilities);
    }
  }

  presentation_capabilities_ = capabilities;

  if (capabilities_changed) {
    for (auto& observer : observers_) {
      observer.OverlayPresentationContextDidChangePresentationCapabilities(
          this);
    }
  }
}

OverlayPresentationContext::UIPresentationCapabilities
OverlayPresentationContextImpl::ConstructPresentationCapabilities() {
  if (ui_disabled_) {
    return UIPresentationCapabilities::kNone;
  }

  UIPresentationCapabilities capabilities = UIPresentationCapabilities::kNone;
  if (container_view_controller_) {
    capabilities = static_cast<UIPresentationCapabilities>(
        capabilities | UIPresentationCapabilities::kContained);
  }
  if (presentation_context_view_controller_) {
    capabilities = static_cast<UIPresentationCapabilities>(
        capabilities | UIPresentationCapabilities::kPresented);
  }
  return capabilities;
}

#pragma mark Presentation and Dismissal helpers

void OverlayPresentationContextImpl::ShowUIForPresentedRequest(
    OverlayRequest* request) {
  const OverlayRequestId request_id = OverlayRequestIdForRequest(request);
  DCHECK_EQ(request_id_, request_id);
  DCHECK(CanShowUIForRequest(request));

  // Create the coordinator if necessary.
  OverlayRequestUIState* state = GetRequestUIState(request_id);
  OverlayRequestCoordinator* overlay_coordinator = state->coordinator();
  UIViewController* base_view_controller = GetBaseViewController(request);
  if (!overlay_coordinator ||
      overlay_coordinator.baseViewController != base_view_controller) {
    overlay_coordinator =
        [coordinator_factory_ newCoordinatorForRequest:request
                                              delegate:&coordinator_delegate_
                                    baseViewController:base_view_controller];
    state->OverlayUIWillBePresented(overlay_coordinator);
  }

  [overlay_coordinator startAnimated:!state->has_ui_been_presented()];
  state->OverlayUIWasPresented();
}

void OverlayPresentationContextImpl::OverlayUIWasPresented() {
  OverlayRequestUIState* state = GetRequestUIState(request_id_);
  DCHECK(state);
  UIView* overlay_view = state->coordinator().viewController.view;
  DCHECK(overlay_view);
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  overlay_view);
}

void OverlayPresentationContextImpl::DismissPresentedUI(
    OverlayDismissalReason reason) {
  OverlayRequestUIState* state = GetRequestUIState(request_id_);
  DCHECK(state);
  DCHECK(state->coordinator());

  state->set_dismissal_reason(reason);
  bool animate_dismissal = reason == OverlayDismissalReason::kUserInteraction;
  [state->coordinator() stopAnimated:animate_dismissal];
}

void OverlayPresentationContextImpl::OverlayUIWasDismissed() {
  DCHECK(request_id_);
  DCHECK(GetRequestUIState(request_id_)->has_callback());
  // If there is another request in the active WebState's OverlayRequestQueue,
  // executing the state's dismissal callback will trigger the presentation of
  // the next request.  If the presented request remains unchanged after calling
  // the dismissal callback, reset it to nullptr since the UI is no longer
  // presented.
  const OverlayRequestId previously_presented_request_id = request_id_;
  GetRequestUIState(request_id_)->OverlayUIWasDismissed();
  if (request_id_ == previously_presented_request_id) {
    SetRequest(nullptr);
  }
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
  const OverlayRequestId request_id = OverlayRequestIdForRequest(request);
  DCHECK_EQ(presentation_context_->request_id_, request_id);
  presentation_context_->OverlayUIWasPresented();
}

void OverlayPresentationContextImpl::OverlayRequestCoordinatorDelegateImpl::
    OverlayUIDidFinishDismissal(OverlayRequest* request) {
  DCHECK(request);
  const OverlayRequestId request_id = OverlayRequestIdForRequest(request);
  DCHECK_EQ(presentation_context_->request_id_, request_id);
  presentation_context_->OverlayUIWasDismissed();
}
