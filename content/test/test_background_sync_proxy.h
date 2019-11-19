// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BACKGROUND_SYNC_PROXY_H_
#define CONTENT_TEST_TEST_BACKGROUND_SYNC_PROXY_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"

#include "content/browser/background_sync/background_sync_proxy.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"

namespace content {

class TestBackgroundSyncProxy : public BackgroundSyncProxy {
 public:
  explicit TestBackgroundSyncProxy(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
      : BackgroundSyncProxy(service_worker_context) {}
  ~TestBackgroundSyncProxy() override;

  void ScheduleDelayedProcessing(blink::mojom::BackgroundSyncType sync_type,
                                 base::TimeDelta delay,
                                 base::OnceClosure delayed_task) override;

  base::OneShotTimer& GetDelayedTimer(
      blink::mojom::BackgroundSyncType sync_type) {
    if (sync_type == blink::mojom::BackgroundSyncType::ONE_SHOT)
      return one_shot_sync_delay_timer_;
    else
      return periodic_sync_delay_timer_;
  }

  void RunDelayedTask(blink::mojom::BackgroundSyncType sync_type) {
    GetDelayedTimer(sync_type).FireNow();
  }

  bool IsDelayedTaskSet(blink::mojom::BackgroundSyncType sync_type) {
    return GetDelayedTimer(sync_type).IsRunning();
  }

  base::TimeDelta GetDelay(blink::mojom::BackgroundSyncType sync_type) {
    return GetDelayedTimer(sync_type).GetCurrentDelay();
  }

 private:
  base::OneShotTimer one_shot_sync_delay_timer_;
  base::OneShotTimer periodic_sync_delay_timer_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BACKGROUND_SYNC_PROXY_H_