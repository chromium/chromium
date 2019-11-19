// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OverlayPresenterObserverBridge::OverlayPresenterObserverBridge(
    id<OverlayPresenterObserving> observer)
    : observer_(observer) {
  DCHECK(observer_);
}

OverlayPresenterObserverBridge::~OverlayPresenterObserverBridge() = default;

void OverlayPresenterObserverBridge::WillShowOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  if ([observer_ respondsToSelector:@selector(overlayPresenter:
                                        willShowOverlayForRequest:)]) {
    [observer_ overlayPresenter:presenter willShowOverlayForRequest:request];
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
