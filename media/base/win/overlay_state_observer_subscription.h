// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_OVERLAY_STATE_OBSERVER_SUBSCRIPTION_H_
#define MEDIA_BASE_WIN_OVERLAY_STATE_OBSERVER_SUBSCRIPTION_H_

#include "base/functional/callback.h"

namespace gpu {
struct Mailbox;
}

namespace media {

// OverlayStateObserverSubscription is an empty interface class which allows a
// media component (e.g. MediaFoundationRendererClient) to manage a reference
// to an OverlayStateObserver implementation where a reference to the concrete
// implementation may not be allowed as a result of the dependency chain (e.g.
// a concrete implementation in //content).
//
// No additional interface methods are required as creation parameters (a
// Mailbox & notification callback) may be provided at the time the object is
// constructed. All further interactions between the OverlayStateObserver and a
// subscribing component flow from the OverlayStateObserver to the subscriber
// via the notification callback using a push model.
class OverlayStateObserverSubscription {
 public:
  virtual ~OverlayStateObserverSubscription() = default;

  // The bool parameter indicates if the surface was promoted to an overlay.
  using StateChangedCB = base::RepeatingCallback<void(bool)>;
};

using ObserveOverlayStateCB = base::RepeatingCallback<
    std::unique_ptr<media::OverlayStateObserverSubscription>(
        const gpu::Mailbox& mailbox,
        OverlayStateObserverSubscription::StateChangedCB on_state_changed_cb)>;

}  // namespace media

#endif  // MEDIA_BASE_WIN_OVERLAY_STATE_OBSERVER_SUBSCRIPTION_H_
