// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

struct ParkableImageManager::Statistics {
  size_t total_size = 0;
};

constexpr const char* ParkableImageManager::kAllocatorDumpName;

// static
ParkableImageManager& ParkableImageManager::Instance() {
  static base::NoDestructor<ParkableImageManager> instance;
  return *instance;
}

bool ParkableImageManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs&,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = pmd->CreateAllocatorDump(kAllocatorDumpName);

  Statistics stats = ComputeStatistics();

  dump->AddScalar("total_size", "bytes", stats.total_size);

  return true;
}

ParkableImageManager::Statistics ParkableImageManager::ComputeStatistics()
    const {
  Statistics stats;

  for (auto* image : images_) {
    stats.total_size += image->size();
  }

  return stats;
}

void ParkableImageManager::Add(ParkableImage* image) {
  DCHECK(IsMainThread());

  if (!has_posted_accounting_task_) {
    auto task_runner = Thread::Current()->GetTaskRunner();
    DCHECK(task_runner);
    // |base::Unretained(this)| is fine because |this| is a NoDestructor
    // singleton.
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ParkableImageManager::RecordStatisticsAfter5Minutes,
                       base::Unretained(this)),
        base::TimeDelta::FromMinutes(5));
    has_posted_accounting_task_ = true;
  }

  images_.insert(image);
}

void ParkableImageManager::Remove(ParkableImage* image) {
  DCHECK(IsMainThread());

  images_.erase(image);
}

void ParkableImageManager::RecordStatisticsAfter5Minutes() const {
  Statistics stats = ComputeStatistics();

  base::UmaHistogramCounts100000("Memory.ParkableImage.TotalSize.5min",
                                 stats.total_size / 1024);  // Record in KiB.
}

}  // namespace blink
