// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache.h"

#include "base/feature_list.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"

namespace content {

FontEnumerationCache::FontEnumerationCache() = default;
FontEnumerationCache::~FontEnumerationCache() = default;

#if !defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
//  static
FontEnumerationCache* FontEnumerationCache::GetInstance() {
  return nullptr;
}
#endif

void FontEnumerationCache::QueueShareMemoryRegionWhenReady(
    scoped_refptr<base::TaskRunner> task_runner,
    blink::mojom::FontAccessManager::EnumerateLocalFontsCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  callbacks_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FontEnumerationCache::RunPendingCallback,
          // Safe because this is an initialized singleton.
          base::Unretained(this),
          CallbackOnTaskRunner(std::move(task_runner), std::move(callback))));

  if (!enumeration_cache_build_started_.IsSet()) {
    enumeration_cache_build_started_.Set();

    SchedulePrepareFontEnumerationCache();
  }
}

bool FontEnumerationCache::IsFontEnumerationCacheReady() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  return enumeration_cache_built_.IsSet() && IsFontEnumerationCacheValid();
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

void FontEnumerationCache::BuildEnumerationCache(
    std::unique_ptr<blink::FontEnumerationTable> table) {
  DCHECK(!enumeration_cache_built_.IsSet());

  enumeration_cache_memory_ =
      base::ReadOnlySharedMemoryRegion::Create(table->ByteSizeLong());

  if (!IsFontEnumerationCacheValid() ||
      !table->SerializeToArray(enumeration_cache_memory_.mapping.memory(),
                               enumeration_cache_memory_.mapping.size())) {
    enumeration_cache_memory_ = base::MappedReadOnlyRegion();
  }

  enumeration_cache_built_.Set();
}

}  // namespace content
