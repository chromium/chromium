// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"

OverlayPresenterObserverBridge::OverlayPresenterObserverBridge(
    id<OverlayPresenterObserving> observer)
    : observer_(observer) {
  DCHECK(observer_);
}

OverlayPresenterObserverBridge::~OverlayPresenterObserverBridge() = default;

const OverlayRequestSupport* OverlayPresenterObserverBridge::GetRequestSupport(
    OverlayPresenter* presenter) const {
  if ([observer_
          respondsToSelector:@selector(overlayRequestSupportForPresenter:)]) {
    return [observer_ overlayRequestSupportForPresenter:presenter];
  }
  return OverlayRequestSupport::All();
}

void OverlayPresenterObserverBridge::WillShowOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request,
    bool initial_presentation) {
  if ([observer_ respondsToSelector:@selector
                 (overlayPresenter:
                     willShowOverlayForRequest:initialPresentation:)]) {
    [observer_ overlayPresenter:presenter
        willShowOverlayForRequest:request
              initialPresentation:initial_presentation];
  }
}

void OverlayPresenterObserverBridge::DidShowOverlay(OverlayPresenter* presenter,
                                                    OverlayRequest* request) {
  if ([observer_ respondsToSelector:@selector(overlayPresenter:
                                        didShowOverlayForRequest:)]) {
    [observer_ overlayPresenter:presenter didShowOverlayForRequest:request];
  }
}

void OverlayPresenterObserverBridge::DidHideOverlay(OverlayPresenter* presenter,
                                                    OverlayRequest* request) {
  if ([observer_ respondsToSelector:@selector(overlayPresenter:
                                        didHideOverlayForRequest:)]) {
    [observer_ overlayPresenter:presenter didHideOverlayForRequest:request];
  }
}

void OverlayPresenterObserverBridge::OverlayPresenterDestroyed(
    OverlayPresenter* presenter) {
  if ([observer_ respondsToSelector:@selector(overlayPresenterDestroyed:)]) {
    [observer_ overlayPresenterDestroyed:presenter];
  }
}
