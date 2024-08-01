// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_IMPL_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/model/public/overlay_user_data.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_fullscreen_disabler.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_ui_state.h"

@class OverlayRequestCoordinatorFactory;
@class OverlayPresentationContextCoordinator;
@protocol OverlayPresentationContextImplDelegate;

// Implementation of OverlayPresentationContext.  An instance of this class
// exists for every OverlayModality for each Browser.  This delegate is scoped
// to the Browser because it needs to store state even when a Browser's UI is
// not on screen.  When a Browser's UI is shown, the view controllers for each
// modality are set up.  When the presentation context is supplied with a
// container or presentation context UIViewController, its presentation
// capabilities are updated and supported overlay UI can begin being shown in
// the context.
class OverlayPresentationContextImpl : public OverlayPresentationContext {
 public:
  // Returns the OverlayPresentationContextImpl for `browser` at `modality`.
  static OverlayPresentationContextImpl* FromBrowser(Browser* browser,
                                                     OverlayModality modality);

  ~OverlayPresentationContextImpl() override;

  // Container that stores the UI delegate for each modality.  Usage example:
  //
  // OverlayPresentationContextImpl::Container::FromUserData(browser)->
  //     PresentationContextForModality(OverlayModality::kWebContentArea);
  class Container : public OverlayUserData<Container> {
   public:
    ~Container() override;

    // Returns the OverlayPresentationContextImpl for `modality`.
    OverlayPresentationContextImpl* PresentationContextForModality(
        OverlayModality modality);

   private:
    OVERLAY_USER_DATA_SETUP(Container);
    explicit Container(Browser* browser);

    raw_ptr<Browser> browser_ = nullptr;
    std::map<OverlayModality, std::unique_ptr<OverlayPresentationContextImpl>>
        ui_delegates_;
  };

  // The context's delegate.
  void SetDelegate(id<OverlayPresentationContextImplDelegate> delegate);

  // The window in which overlay UI will be presented.
  void SetWindow(UIWindow* window);

  // The UIViewController used for overlays displayed using child
  // UIViewControllers.  Setting to a new value updates the presentation
  // capabilities to include kContained.
  void SetContainerViewController(UIViewController* view_controller);

  // The UIViewController used for overlays displayed using presented
  // UIViewControllers.  Setting to a new value updates the presentation
  // capabilities to include kPresented.
  void SetPresentationContextViewController(UIViewController* view_controller);

  // OverlayPresentationContext:
  void AddObserver(OverlayPresentationContextObserver* observer) override;
  void RemoveObserver(OverlayPresentationContextObserver* observer) override;
  UIPresentationCapabilities GetPresentationCapabilities() const override;
  bool CanShowUIForRequest(
      OverlayRequest* request,
      UIPresentationCapabilities capabilities) const override;
  bool CanShowUIForRequest(OverlayRequest* request) const override;
  bool IsShowingOverlayUI() const override;
  void PrepareToShowOverlayUI(OverlayRequest* request) override;
  void ShowOverlayUI(OverlayRequest* request,
                     OverlayPresentationCallback presentation_callback,
                     OverlayDismissalCallback dismissal_callback) override;
  void HideOverlayUI(OverlayRequest* request) override;
  void CancelOverlayUI(OverlayRequest* request) override;
  void SetUIDisabled(bool disabled) override;
  bool IsUIDisabled() override;

 protected:
  // Constructor called by the Container to instantiate a presentation context
  // for `browser` at `modality`, using `factory` to create
  // OverlayRequestCoordinators.
  OverlayPresentationContextImpl(Browser* browser,
                                 OverlayModality modality,
                                 OverlayRequestCoordinatorFactory* factory);

 private:
  // Setter for `request_`.  Setting to a new value will attempt to
  // present the UI for `request`.
  void SetRequest(OverlayRequest* request);

  // Returns whether `request` uses a child UIViewController.  If false, the
  // request's UI is shown using presentation.
  bool RequestUsesChildViewController(OverlayRequest* request) const;

  // Returns the base view controller to use for `request`'s coordinator, or
  // nullptr if the base has not been provided.  `container_view_controller_` is
  // returned if `request` uses a child UIViewController, and
  // `presentation_context_view_controller_` is returned if `request` uses
  // UIViewController presentation.
  UIViewController* GetBaseViewController(OverlayRequest* request) const;

  // Returns the UI state for `request`.
  OverlayRequestUIState* GetRequestUIState(OverlayRequest* request) const;

  // Returns the presentation capabilities required to show `request`.
  UIPresentationCapabilities GetRequiredPresentationCapabilities(
      OverlayRequest* request) const;

  // Updates the presentation capabilities based on the provided
  // UIViewControllers.
  void UpdatePresentationCapabilities();

  // Creates the current UIPresentationCapabilities based on the current state.
  UIPresentationCapabilities ConstructPresentationCapabilities();

  // Shows the UI for the presented request using the container coordinator.
  void ShowUIForPresentedRequest();

  // Called when the UI for `request_` has finished being presented.
  void OverlayUIWasPresented();

  // Dismisses the UI for the presented request for `reason`.
  void DismissPresentedUI(OverlayDismissalReason reason);

  // Called when the UI for `request_` has finished being dismissed.
  void OverlayUIWasDismissed();

  // Called when the Browser is being destroyed.
  void BrowserDestroyed();

  // Helper object that detaches the UI delegate for Browser shudown.
  class BrowserShutdownHelper : public BrowserObserver {
   public:
    BrowserShutdownHelper(Browser* browser,
                          OverlayPresenter* presenter,
                          OverlayPresentationContextImpl* presentation_context);
    ~BrowserShutdownHelper() override;

    // BrowserObserver:
    void BrowserDestroyed(Browser* browser) override;

   private:
    // The presenter whose delegate needs to be reset.
    raw_ptr<OverlayPresenter> presenter_ = nullptr;
    // OverlayPresentationContextImpl reference.
    raw_ptr<OverlayPresentationContextImpl> presentation_context_ = nullptr;
    // Scoped observation.
    base::ScopedObservation<Browser, BrowserObserver> browser_observation_{
        this};
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
    raw_ptr<OverlayPresentationContextImpl> presentation_context_ = nullptr;
  };

  // The presenter whose UI is being handled by this delegate.
  raw_ptr<OverlayPresenter> presenter_ = nullptr;
  // The cleanup helper.
  BrowserShutdownHelper shutdown_helper_;
  // The delegate used to intercept presentation/dismissal events from
  // OverlayRequestCoordinators.
  OverlayRequestCoordinatorDelegateImpl coordinator_delegate_;
  // The fullscreen disabler helper.
  OverlayContainerFullscreenDisabler fullscreen_disabler_;
  // The coordinator factory that provides the UI for the overlays at this
  // modality.
  OverlayRequestCoordinatorFactory* coordinator_factory_ = nil;
  // The context's delegate.
  __weak id<OverlayPresentationContextImplDelegate> delegate_ = nil;
  // The window in which overlay UI will be presented.
  __weak UIWindow* window_ = nil;
  // The UIViewController used as the base for overlays UI displayed using child
  // UIViewControllers.
  __weak UIViewController* container_view_controller_ = nil;
  // The UIViewController used as the base for overlays displayed using
  // presented UIViewControllers.
  __weak UIViewController* presentation_context_view_controller_ = nil;
  // Whether the UI is temporarily disabled.
  bool ui_disabled_ = false;
  // The presentation capabilities of `coordinator_`'s view controller.
  UIPresentationCapabilities presentation_capabilities_ =
      UIPresentationCapabilities::kNone;
  // The request that is currently presented by `presenter_`.  When a new
  // request is presented, the UI state for the request will be added to
  // `states_`.
  raw_ptr<OverlayRequest> request_ = nullptr;
  // Map storing the UI state for each OverlayRequest.
  std::map<OverlayRequest*, std::unique_ptr<OverlayRequestUIState>> states_;
  base::ObserverList<OverlayPresentationContextObserver,
                     /* check_empty= */ true>
      observers_;
  // Weak pointer factory.
  base::WeakPtrFactory<OverlayPresentationContextImpl> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_IMPL_H_
