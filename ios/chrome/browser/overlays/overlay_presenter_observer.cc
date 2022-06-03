// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/overlay_presenter_observer.h"

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"

OverlayPresenterObserver::OverlayPresenterObserver() = default;

const OverlayRequestSupport* OverlayPresenterObserver::GetRequestSupport(
    OverlayPresenter* presenter) const {
  return OverlayRequestSupport::All();
}
