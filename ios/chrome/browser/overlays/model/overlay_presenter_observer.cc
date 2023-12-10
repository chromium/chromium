// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/public/overlay_presenter_observer.h"

#include <ostream>

#include "base/check.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_support.h"

OverlayPresenterObserver::OverlayPresenterObserver() = default;

OverlayPresenterObserver::~OverlayPresenterObserver() {
  CHECK(!IsInObserverList())
      << "OverlayPresenterObserver needs to be removed from OverlayPresenter "
         "observer list before their destruction.";
}

const OverlayRequestSupport* OverlayPresenterObserver::GetRequestSupport(
    OverlayPresenter* presenter) const {
  return OverlayRequestSupport::All();
}
