// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_
#define CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_

#include "base/containers/linked_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/idle/idle_monitor.h"
#include "content/browser/idle/idle_polling_service.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT IdleManagerImpl : public blink::mojom::IdleManager,
                                       public IdlePollingService::Observer {
 public:
  explicit IdleManagerImpl(RenderFrameHost* render_frame_host);
  ~IdleManagerImpl() override;

  IdleManagerImpl(const IdleManagerImpl&) = delete;
  IdleManagerImpl& operator=(const IdleManagerImpl&) = delete;

  void CreateService(mojo::PendingReceiver<blink::mojom::IdleManager> receiver);

  // blink.mojom.IdleManager:
  void AddMonitor(base::TimeDelta threshold,
                  mojo::PendingRemote<blink::mojom::IdleMonitor> monitor_remote,
                  AddMonitorCallback callback) final;

  void SetIdleOverride(blink::mojom::UserIdleState user_state,
                       blink::mojom::ScreenIdleState screen_state);
  void ClearIdleOverride();

 private:
  // Check permission controller to see if the notification permission is
  // enabled for this frame.
  bool HasPermission();

  // Called internally when a monitor's pipe closes to remove it from
  // |monitors_|.
  void RemoveMonitor(IdleMonitor* monitor);

  // Notifies |monitors_| of the new idle state.
  void OnIdleStateChange(const IdlePollingService::State& state) override;

  blink::mojom::IdleStatePtr CheckIdleState(base::TimeDelta threshold);

  base::ScopedObservation<IdlePollingService, IdlePollingService::Observer>
      observer_{this};

  blink::mojom::IdleStatePtr state_override_;

  // Raw pointer is safe because this object is owned by |render_frame_host_|.
  RenderFrameHost* const render_frame_host_;

  // Registered clients.
  mojo::ReceiverSet<blink::mojom::IdleManager> receivers_;

  // Owns Monitor instances, added when clients call AddMonitor().
  base::LinkedList<IdleMonitor> monitors_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IdleManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_
