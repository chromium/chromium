// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IDLE_IDLE_MANAGER_H_
#define CONTENT_BROWSER_IDLE_IDLE_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/idle/idle_monitor.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "ui/base/idle/idle.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT IdleManager : public blink::mojom::IdleManager {
 public:
  // This class adapts functions from ui:: and allows tests to
  // inject custom providers.
  // Adapted from: extensions/browser/api/idle/idle_manager.h
  class IdleTimeProvider {
   public:
    using IdleCallback = base::OnceCallback<void(blink::mojom::IdleState)>;
    using IdleTimeCallback = base::OnceCallback<void(int)>;

    IdleTimeProvider() {}
    virtual ~IdleTimeProvider() {}

    // See ui/base/idle/idle.h for the semantics of these methods.
    // TODO(goto): should this be made private? Doesn't seem to be necessary
    // as part of a public interface.
    virtual ui::IdleState CalculateIdleState(
        base::TimeDelta idle_threshold) = 0;
    virtual base::TimeDelta CalculateIdleTime() = 0;
    virtual bool CheckIdleStateIsLocked() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(IdleTimeProvider);
  };

  IdleManager();
  ~IdleManager() override;

  void CreateService(blink::mojom::IdleManagerRequest request);

  // blink.mojom.IdleManager:
  void AddMonitor(base::TimeDelta threshold,
                  mojo::PendingRemote<blink::mojom::IdleMonitor> monitor_remote,
                  AddMonitorCallback callback) override;

  // Testing helpers.
  void SetIdleTimeProviderForTest(
      std::unique_ptr<IdleTimeProvider> idle_provider);

  // Tests whether the manager is still polling for updates or not.
  bool IsPollingForTest();

 private:
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

  // Registered clients.
  mojo::BindingSet<blink::mojom::IdleManager> bindings_;

  // Owns Monitor instances, added when clients call AddMonitor().
  base::LinkedList<IdleMonitor> monitors_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IdleManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IdleManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_IDLE_IDLE_MANAGER_H_
