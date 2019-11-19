// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_IMPL_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/main/browser_observer.h"
#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/public/overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/public/overlay_user_data.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_ui_state.h"

@class OverlayRequestCoordinatorFactory;
@class OverlayContainerCoordinator;

// Implementation of OverlayPresentationContext.  An instance of this class
// exists for every OverlayModality for each Browser.  This delegate is scoped
// to the Browser because it needs to store state even when a Browser's UI is
// not on screen.  When a Browser's UI is shown, the OverlayContainerCoordinator
// for each of its OverlayModalities will supply itself to the delegate, which
// will then present the UI using the container coordinator's presentation
// context.
class OverlayPresentationContextImpl : public OverlayPresentationContext {
 public:
  ~OverlayPresentationContextImpl() override;

  // Container that stores the UI delegate for each modality.  Usage example:
  //
  // OverlayPresentationContextImpl::Container::FromUserData(browser)->
  //     PresentationContextForModality(OverlayModality::kWebContentArea);
  class Container : public OverlayUserData<Container> {
   public:
    ~Container() override;

    // Returns the OverlayPresentationContextImpl for |modality|.
    OverlayPresentationContextImpl* PresentationContextForModality(
        OverlayModality modality);

   private:
    OVERLAY_USER_DATA_SETUP(Container);
    explicit Container(Browser* browser);

    Browser* browser_ = nullptr;
    std::map<OverlayModality, std::unique_ptr<OverlayPresentationContextImpl>>
        ui_delegates_;
  };

  // The OverlayContainerCoordinator is used to present the overlay UI at the
  // correct modality in the app.  Should only be set when the coordinator is
  // started.
  OverlayContainerCoordinator* coordinator() const { return coordinator_; }
  void SetCoordinator(OverlayContainerCoordinator* coordinator);

  // Called when |coordinator_|'s view was moved to a new window.
  void WindowDidChange();

  // OverlayPresentationContext:
  void AddObserver(OverlayPresentationContextObserver* observer) override;
  void RemoveObserver(OverlayPresentationContextObserver* observer) override;
  UIPresentationCapabilities GetPresentationCapabilities() const override;
  bool CanShowUIForRequest(
      OverlayRequest* request,
      UIPresentationCapabilities capabilities) const override;
  bool CanShowUIForRequest(OverlayRequest* request) const override;
  void ShowOverlayUI(OverlayPresenter* presenter,
                     OverlayRequest* request,
                     OverlayPresentationCallback presentation_callback,
                     OverlayDismissalCallback dismissal_callback) override;
  void HideOverlayUI(OverlayPresenter* presenter,
                     OverlayRequest* request) override;
  void CancelOverlayUI(OverlayPresenter* presenter,
                       OverlayRequest* request) override;

 private:
  OverlayPresentationContextImpl(Browser* browser, OverlayModality modality);

  // Setter for |request_|.  Setting to a new value will attempt to
  // present the UI for |request|.
  void SetRequest(OverlayRequest* request);

  // Returns the UI state for |request|.
  OverlayRequestUIState* GetRequestUIState(OverlayRequest* request);

  // Updates |coordinator_| and |presentation_capabilities_| using
  // |coordinator|.
  void UpdateForCoordinator(OverlayContainerCoordinator* coordinator);

  // Shows the UI for the presented request using the container coordinator.
  void ShowUIForPresentedRequest();

  // Called when the UI for |request_| has finished being presented.
  void OverlayUIWasPresented();

  // Dismisses the UI for the presented request for |reason|.
  void DismissPresentedUI(OverlayDismissalReason reason);

  // Called when the UI for |request_| has finished being dismissed.
  void OverlayUIWasDismissed();

  // Helper object that detaches the UI delegate for Browser shudown.
  class BrowserShutdownHelper : public BrowserObserver {
   public:
    BrowserShutdownHelper(Browser* browser, OverlayPresenter* presenter);
    ~BrowserShutdownHelper() override;

    // BrowserObserver:
    void BrowserDestroyed(Browser* browser) override;

   private:
    // The presenter whose delegate needs to be reset.
    OverlayPresenter* presenter_ = nullptr;
  };

  // Helper object that listens for UI dismissal events.
  class OverlayRequestCoordinatorDelegateImpl
      : public OverlayRequestCoordinatorDelegate {
   public:
    OverlayRequestCoordinatorDelegateImpl(
        OverlayPresentationContextImpl* presentation_context);
    ~OverlayRequestCoordinatorDelegateImpl() override;

    // OverlayUIDismissalDelegate:
    void OverlayUIDidFinishPresentation(OverlayRequest* request) override;
    void OverlayUIDidFinishDismissal(OverlayRequest* request) override;

   private:
    OverlayPresentationContextImpl* presentation_context_ = nullptr;
  };

  // The presenter whose UI is being handled by this delegate.
  OverlayPresenter* presenter_ = nullptr;
  // The cleanup helper.
  BrowserShutdownHelper shutdown_helper_;
  // The delegate used to intercept presentation/dismissal events from
  // OverlayRequestCoordinators.
  OverlayRequestCoordinatorDelegateImpl coordinator_delegate_;
  // The coordinator factory that provides the UI for the overlays at this
  // modality.
  OverlayRequestCoordinatorFactory* coordinator_factory_ = nil;
  // The coordinator responsible for presenting the UI delegate's UI.
  OverlayContainerCoordinator* coordinator_ = nil;
  // The presentation capabilities of |coordinator_|'s view controller.
  UIPresentationCapabilities presentation_capabilities_ =
      UIPresentationCapabilities::kNone;
  // The request that is currently presented by |presenter_|.  The UI for this
  // request might not yet be visible if no OverlayContainerCoordinator has been
  // provided.  When a new request is presented, the UI state for the request
  // will be added to |states_|.
  OverlayRequest* request_ = nullptr;
  // Map storing the UI state for each OverlayRequest.
  std::map<OverlayRequest*, std::unique_ptr<OverlayRequestUIState>> states_;
  base::ObserverList<OverlayPresentationContextObserver,
                     /* check_empty= */ true>
      observers_;
  // Weak pointer factory.
  base::WeakPtrFactory<OverlayPresentationContextImpl> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_IMPL_H_
