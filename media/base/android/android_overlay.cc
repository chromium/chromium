// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/android_overlay.h"

namespace media {

AndroidOverlay::AndroidOverlay() {}
AndroidOverlay::~AndroidOverlay() {
  // Don't permit any other callbacks once we start sending deletion cbs.
  weak_factory_.InvalidateWeakPtrs();
  for (auto& cb : deletion_cbs_)
    std::move(cb).Run(this);
}

void AndroidOverlay::AddSurfaceDestroyedCallback(
    AndroidOverlayConfig::DestroyedCB cb) {
  destruction_cbs_.push_back(std::move(cb));
}

void AndroidOverlay::RunSurfaceDestroyedCallbacks() {
  if (destruction_cbs_.empty())
    return;

  // Move the list, in case it's modified during traversal.
  std::list<AndroidOverlayConfig::DestroyedCB> cbs =
      std::move(destruction_cbs_);

  // Get a wp for |this|, in case it's destroyed during a callback.
  base::WeakPtr<AndroidOverlay> wp = weak_factory_.GetWeakPtr();

  for (auto& cb : cbs) {
    std::move(cb).Run(this);
    // If |this| has been deleted, then stop here.
    if (!wp)
      return;
  }
}

void AndroidOverlay::AddOverlayDeletedCallback(
    AndroidOverlayConfig::DeletedCB cb) {
  deletion_cbs_.push_back(std::move(cb));
}

}  // namespace media
