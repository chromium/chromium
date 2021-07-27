// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_cache.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// static
base::SequenceBound<FontEnumerationCache> FontEnumerationCache::Create() {
  return FontEnumerationCache::CreateForTesting(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      absl::nullopt);
}

#if !defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)

// static
base::SequenceBound<FontEnumerationCache>
FontEnumerationCache::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    absl::optional<std::string> locale_override) {
  return base::SequenceBound<FontEnumerationCache>();
}

#endif  // !defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)

FontEnumerationCache::FontEnumerationCache(
    absl::optional<std::string> locale_override)
    : locale_override_(std::move(locale_override)),
      enumeration_cache_built_(std::make_unique<base::AtomicFlag>()),
      enumeration_cache_build_started_(std::make_unique<base::AtomicFlag>()) {}

FontEnumerationCache::~FontEnumerationCache() = default;

void FontEnumerationCache::QueueShareMemoryRegionWhenReady(
    scoped_refptr<base::TaskRunner> task_runner,
    CacheTaskCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  callbacks_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FontEnumerationCache::RunPendingCallback,
          // Safe because this is an initialized singleton.
          base::Unretained(this),
          CallbackOnTaskRunner(std::move(task_runner), std::move(callback))));

  if (!enumeration_cache_build_started_->IsSet()) {
    enumeration_cache_build_started_->Set();

    SchedulePrepareFontEnumerationCache();
  }
}

bool FontEnumerationCache::IsFontEnumerationCacheReady() {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));

  return enumeration_cache_built_->IsSet() && IsFontEnumerationCacheValid();
}

base::ReadOnlySharedMemoryRegion FontEnumerationCache::DuplicateMemoryRegion() {
  DCHECK(IsFontEnumerationCacheReady());
  return enumeration_cache_memory_.region.Duplicate();
}

FontEnumerationCache::CallbackOnTaskRunner::CallbackOnTaskRunner(
    scoped_refptr<base::TaskRunner> runner,
    CacheTaskCallback callback)
    : task_runner(std::move(runner)), callback(std::move(callback)) {}

FontEnumerationCache::CallbackOnTaskRunner::CallbackOnTaskRunner(
    CallbackOnTaskRunner&& other) = default;

FontEnumerationCache::CallbackOnTaskRunner::~CallbackOnTaskRunner() = default;

void FontEnumerationCache::RunPendingCallback(
    CallbackOnTaskRunner pending_callback) {
  DCHECK(callbacks_task_runner_->RunsTasksInCurrentSequence());

  pending_callback.task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(pending_callback.callback), status_,
                                DuplicateMemoryRegion()));
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
  DCHECK(!enumeration_cache_built_->IsSet());

  // Postscript names, according to spec, are expected to be encoded in a subset
  // of ASCII. See:
  // https://docs.microsoft.com/en-us/typography/opentype/spec/name This is why
  // a "simple" byte-wise comparison is used.
  std::sort(table->mutable_fonts()->begin(), table->mutable_fonts()->end(),
            [](const blink::FontEnumerationTable_FontMetadata& a,
               const blink::FontEnumerationTable_FontMetadata& b) {
              return a.postscript_name() < b.postscript_name();
            });

  enumeration_cache_memory_ =
      base::ReadOnlySharedMemoryRegion::Create(table->ByteSizeLong());

  if (!IsFontEnumerationCacheValid() ||
      !table->SerializeToArray(enumeration_cache_memory_.mapping.memory(),
                               enumeration_cache_memory_.mapping.size())) {
    enumeration_cache_memory_ = base::MappedReadOnlyRegion();
  }

  enumeration_cache_built_->Set();
}

}  // namespace content
