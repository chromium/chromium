// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/private_memory_footprint_provider.h"

#include "base/task/task_runner.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

void PrivateMemoryFootprintProvider::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DEFINE_STATIC_LOCAL(PrivateMemoryFootprintProvider, provider,
                      (std::move(task_runner)));
  (void)provider;
}

PrivateMemoryFootprintProvider::PrivateMemoryFootprintProvider(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : PrivateMemoryFootprintProvider(std::move(task_runner),
                                     base::DefaultTickClock::GetInstance()) {}

PrivateMemoryFootprintProvider::PrivateMemoryFootprintProvider(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* clock)
    : task_runner_(std::move(task_runner)), clock_(clock) {
  auto& monitor = MemoryUsageMonitor::Instance();
  monitor.AddObserver(this);
  OnMemoryPing(monitor.GetCurrentMemoryUsage());
}

void PrivateMemoryFootprintProvider::OnMemoryPing(MemoryUsage usage) {
  DCHECK(IsMainThread());
  SetPrivateMemoryFootprint(
      static_cast<uint64_t>(usage.private_footprint_bytes));
}

void PrivateMemoryFootprintProvider::SetPrivateMemoryFootprint(
    uint64_t private_memory_footprint_bytes) {
  Platform::Current()->SetPrivateMemoryFootprint(
      private_memory_footprint_bytes);
}

}  // namespace blink
