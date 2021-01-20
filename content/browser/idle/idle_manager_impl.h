// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_
#define CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/idle/idle_monitor.h"
#include "content/public/browser/idle_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "url/origin.h"

namespace content {

class BrowserContext;

class CONTENT_EXPORT IdleManagerImpl : public IdleManager,
                                       public blink::mojom::IdleManager {
 public:
  explicit IdleManagerImpl(BrowserContext* browser_context);
  ~IdleManagerImpl() override;

  IdleManagerImpl(const IdleManagerImpl&) = delete;
  IdleManagerImpl& operator=(const IdleManagerImpl&) = delete;

  // IdleManager:
  void CreateService(mojo::PendingReceiver<blink::mojom::IdleManager> receiver,
                     const url::Origin& origin) override;
  void SetIdleTimeProviderForTest(
      std::unique_ptr<IdleTimeProvider> idle_provider) override;
  bool IsPollingForTest() override;

  // blink.mojom.IdleManager:
  void AddMonitor(base::TimeDelta threshold,
                  mojo::PendingRemote<blink::mojom::IdleMonitor> monitor_remote,
                  AddMonitorCallback callback) final;

  void SetIdleOverride(blink::mojom::UserIdleState user_state,
                       blink::mojom::ScreenIdleState screen_state) override;
  void ClearIdleOverride() override;

 private:
  // Check permission controller to see if the notification permission is
  // enabled for the origin.
  bool HasPermission(const url::Origin&);

  // Called internally when a monitor's pipe closes to remove it from
  // |monitors_|.
  void RemoveMonitor(IdleMonitor* monitor);

  // Called internally when a monitor is added via AddMonitor() to maybe
  // start the polling timer, if not already started.
  void StartPolling();

  // Called internally by the timer callback to stop the timer if there
  // are no more monitors. (It is not called from RemoveMonitor() so
  // that an calls can update the cached state.)
  void StopPolling();

  // Callback for the polling timer. Kicks off an async query for the state.
  void UpdateIdleState();

  // Callback for the async state query. Updates monitors as needed.
  void UpdateIdleStateCallback(int idle_time);

  blink::mojom::IdleStatePtr CheckIdleState(base::TimeDelta threshold);

  base::RepeatingTimer poll_timer_;
  std::unique_ptr<IdleTimeProvider> idle_time_provider_;

  blink::mojom::IdleStatePtr state_override_;

  // Raw pointer should always be valid. IdleManagerImpl is owned by the
  // StoragePartitionImpl which is owned by BrowserContext. Therefore when the
  // BrowserContext is destroyed, |this| will be destroyed as well.
  BrowserContext* const browser_context_;

  // Registered clients.
  mojo::ReceiverSet<blink::mojom::IdleManager, url::Origin> receivers_;

  // Owns Monitor instances, added when clients call AddMonitor().
  base::LinkedList<IdleMonitor> monitors_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IdleManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_IDLE_IDLE_MANAGER_IMPL_H_
