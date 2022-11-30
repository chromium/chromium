// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WIN_OVERLAY_STATE_OBSERVER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WIN_OVERLAY_STATE_OBSERVER_IMPL_H_

#include "content/common/content_export.h"
#include "content/renderer/media/win/overlay_state_service_provider.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "media/base/win/overlay_state_observer_subscription.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// OverlayStateObserverImpl provides a simple notification API wrapping
// promotion hints from the OverlayStateService for a single mailbox.
// When a client creates the OverlayState a mailbox they want to observe is
// set at construction time. When a promotion hint change is received from the
// OverlayStateService the StateChangedCB is invoked to inform the client of
// the new promotion state.
class CONTENT_EXPORT OverlayStateObserverImpl
    : public media::OverlayStateObserverSubscription,
      public gpu::mojom::OverlayStateObserver {
 public:
  static std::unique_ptr<media::OverlayStateObserverSubscription> Create(
      OverlayStateServiceProvider* overlay_state_service_provider,
      const gpu::Mailbox& mailbox,
      StateChangedCB state_changed_cb);

  // gpu::mojom::OverlayStateObserver
  // When the OverlayStateService has notified us of a change in
  // promotion status, invoke the StateChangedCB to notify the
  // client.
  void OnStateChanged(bool promoted) override;

 private:
  friend struct std::default_delete<OverlayStateObserverImpl>;
  OverlayStateObserverImpl(
      OverlayStateServiceProvider* overlay_state_service_provider,
      const gpu::Mailbox& mailbox,
      StateChangedCB state_changed_cb);
  OverlayStateObserverImpl(const OverlayStateObserverImpl&) = delete;
  OverlayStateObserverImpl& operator=(const OverlayStateObserverImpl&) = delete;
  ~OverlayStateObserverImpl() override;

  StateChangedCB callback_;
  mojo::Receiver<gpu::mojom::OverlayStateObserver> receiver_;

  base::WeakPtrFactory<OverlayStateObserverImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WIN_OVERLAY_STATE_OBSERVER_IMPL_H_
