// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "content/browser/font_access/font_enumeration_cache_win.h"
#endif

namespace content {

FontEnumerationCache::FontEnumerationCache() = default;
FontEnumerationCache::~FontEnumerationCache() = default;

// static
FontEnumerationCache* FontEnumerationCache::GetInstance() {
#if defined(OS_WIN)
  return FontEnumerationCacheWin::GetInstance();
#endif

  return nullptr;
}

void FontEnumerationCache::ResetStateForTesting() {
  callbacks_task_runner_ =
      base::MakeRefCounted<base::DeferredSequencedTaskRunner>();
  enumeration_cache_memory_ = base::MappedReadOnlyRegion();
  enumeration_cache_built_.UnsafeResetForTesting();
  enumeration_cache_build_started_.UnsafeResetForTesting();
  status_ = FontEnumerationStatus::kOk;
}

base::ReadOnlySharedMemoryRegion FontEnumerationCache::DuplicateMemoryRegion() {
  DCHECK(IsFontEnumerationCacheReady());
  return enumeration_cache_memory_.region.Duplicate();
}

FontEnumerationCache::CallbackOnTaskRunner::CallbackOnTaskRunner(
    scoped_refptr<base::TaskRunner> runner,
    blink::mojom::FontAccessManager::EnumerateLocalFontsCallback callback)
    : task_runner(std::move(runner)), mojo_callback(std::move(callback)) {}

FontEnumerationCache::CallbackOnTaskRunner::CallbackOnTaskRunner(
    CallbackOnTaskRunner&& other) = default;

FontEnumerationCache::CallbackOnTaskRunner::~CallbackOnTaskRunner() = default;

void FontEnumerationCache::RunPendingCallback(
    CallbackOnTaskRunner pending_callback) {
  DCHECK(callbacks_task_runner_->RunsTasksInCurrentSequence());

  pending_callback.task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(pending_callback.mojo_callback),
                                status_, DuplicateMemoryRegion()));
}

void FontEnumerationCache::StartCallbacksTaskQueue() {
  callbacks_task_runner_->StartWithTaskRunner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT}));
}

bool FontEnumerationCache::IsFontEnumerationCacheValid() const {
  return enumeration_cache_memory_.IsValid() &&
         enumeration_cache_memory_.mapping.size();
}

}  // namespace content
