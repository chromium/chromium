// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/overlays/model/public/overlay_presenter_observer.h"

// Observes overlay UI presentation events from Objective-C. To use as an
// OverlayPresenterObserver, wrap in an OverlayPresenterObserverBridge.
@protocol OverlayPresenterObserving <NSObject>
@optional

// Invoked by OverlayPresenterObserver::GetOverlayRequestSupport().
- (const OverlayRequestSupport*)overlayRequestSupportForPresenter:
    (OverlayPresenter*)presenter;

// Invoked by OverlayPresenterObserver::WillShowOverlay().
- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation;

// Invoked by OverlayPresenterObserver::DidShowOverlay().
- (void)overlayPresenter:(OverlayPresenter*)presenter
    didShowOverlayForRequest:(OverlayRequest*)request;

// Invoked by OverlayPresenterObserver::DidHideOverlay().
- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request;

// Invoked by OverlayPresenterObserver::OverlayPresenterDestroyed().
- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter;

@end

// C++ wrapper that forwards OverlayPresenterObserver callbacks to an Objective-
// C object conforming to OverlayPresenterObserving.
class OverlayPresenterObserverBridge : public OverlayPresenterObserver {
 public:
  // It it the responsibility of calling code to add/remove the instance
  // from OverlayPresenter's observer list.
  OverlayPresenterObserverBridge(id<OverlayPresenterObserving> observer);

  OverlayPresenterObserverBridge(const OverlayPresenterObserverBridge&) =
      delete;
  OverlayPresenterObserverBridge& operator=(
      const OverlayPresenterObserverBridge&) = delete;

  ~OverlayPresenterObserverBridge() override;

  // OverlayPresenterObserver:
  const OverlayRequestSupport* GetRequestSupport(
      OverlayPresenter* presenter) const override;
  void WillShowOverlay(OverlayPresenter* presenter,
                       OverlayRequest* request,
                       bool initial_presentation) override;
  void DidShowOverlay(OverlayPresenter* presenter,
                      OverlayRequest* request) override;
  void DidHideOverlay(OverlayPresenter* presenter,
                      OverlayRequest* request) override;
  void OverlayPresenterDestroyed(OverlayPresenter* presenter) override;

 private:
  __weak id<OverlayPresenterObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTER_OBSERVER_BRIDGE_H_
