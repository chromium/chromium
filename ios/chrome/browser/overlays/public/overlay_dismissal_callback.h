// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_DISMISSAL_CALLBACK_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_DISMISSAL_CALLBACK_H_

#include "base/callback.h"

// Enum type categorizing the different reasons overlay UI may be dismissed.
// Used in OverlayDismissalCallbacks to notify the OverlayPresenter why the
// overlay UI was dismissed.
enum class OverlayDismissalReason {
  // Used when the overlay UI is dismissed by the user.
  kUserInteraction,
  // Used when the overlay is hidden by the presenter.
  kHiding,
  // Used when the overlay is cancelled by the presenter.
  kCancellation,
};

// Overlay UI presented by OverlayPresenter::Delegate are provided with an
// OverlayDismissalCallback that is used to notify the presenter when requested
// overlay UI has finished being dismissed.  |reason| is used to communicate
// what triggered the dismissal.  Overlays that are hidden may be shown again,
// so the callback will not update the OverlayRequestQueue. Overlays dismissed
// for user interaction will never be shown again; executing the dismissal
// callback for this reason will execute the request's callback and remove it
// from its queue.
typedef base::OnceCallback<void(OverlayDismissalReason reason)>
    OverlayDismissalCallback;

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_DISMISSAL_CALLBACK_H_
