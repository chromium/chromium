// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_
#define CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "ui/base/idle/idle_polling_service.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT IdleManagerImpl : public blink::mojom::IdleManager,
                                       public ui::IdlePollingService::Observer {
 public:
  explicit IdleManagerImpl(RenderFrameHost* render_frame_host);
  ~IdleManagerImpl() override;

  IdleManagerImpl(const IdleManagerImpl&) = delete;
  IdleManagerImpl& operator=(const IdleManagerImpl&) = delete;

  void CreateService(mojo::PendingReceiver<blink::mojom::IdleManager> receiver);

  // blink.mojom.IdleManager:
  void AddMonitor(mojo::PendingRemote<blink::mojom::IdleMonitor> monitor_remote,
                  AddMonitorCallback callback) final;

  void SetIdleOverride(bool is_user_active, bool is_screen_unlocked);
  void ClearIdleOverride();

 private:
  // Check permission controller to see if the notification permission is
  // enabled for this frame.
  bool HasPermission();

  // When a monitor's pipe closes and it has been removed from |monitors_|.
  void OnMonitorDisconnected(mojo::RemoteSetElementId id);

  // Notifies |monitors_| of the new idle state.
  void OnIdleStateChange(const ui::IdlePollingService::State& state) override;

  blink::mojom::IdleStatePtr CreateIdleState(
      const ui::IdlePollingService::State& state);
  blink::mojom::IdleStatePtr CheckIdleState();

  // |observer_| and |state_override_| are mutually exclusive as when DevTools
  // has provided an override we no longer need to poll for the actual state.
  base::ScopedObservation<ui::IdlePollingService,
                          ui::IdlePollingService::Observer>
      observer_{this};
  bool state_override_ = false;

  // Raw pointer is safe because this object is owned by |render_frame_host_|.
  const raw_ptr<RenderFrameHost> render_frame_host_;

  // Registered clients.
  mojo::ReceiverSet<blink::mojom::IdleManager> receivers_;

  blink::mojom::IdleStatePtr last_state_;

  // Registered IdleMonitor instances, added when clients call AddMonitor().
  mojo::RemoteSet<blink::mojom::IdleMonitor> monitors_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IdleManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_
