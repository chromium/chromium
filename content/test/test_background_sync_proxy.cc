// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_background_sync_proxy.h"

namespace content {

TestBackgroundSyncProxy::~TestBackgroundSyncProxy() {}

void TestBackgroundSyncProxy::ScheduleDelayedProcessing(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay,
    base::OnceClosure delayed_task) {
  auto& delay_timer = GetDelayedTimer(sync_type);
  if (delay.is_max())
    delay_timer.AbandonAndStop();
  else if (!delay.is_zero())
    delay_timer.Start(FROM_HERE, delay, std::move(delayed_task));
}

}  // namespace content
