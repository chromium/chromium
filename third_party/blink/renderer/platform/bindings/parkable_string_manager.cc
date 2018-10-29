// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

ParkableStringManager& ParkableStringManager::Instance() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(ParkableStringManager, instance, ());
  return instance;
}

ParkableStringManager::~ParkableStringManager() {}

void ParkableStringManager::SetRendererBackgrounded(bool backgrounded) {
  backgrounded_ = backgrounded;

  if (backgrounded_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        Platform::Current()->CurrentThread()->GetTaskRunner();
    DCHECK(task_runner);
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ParkableStringManager::ParkAllIfRendererBackgrounded,
                       base::Unretained(this)),
        base::TimeDelta::FromSeconds(10));
  }
}

bool ParkableStringManager::IsRendererBackgrounded() const {
  return backgrounded_;
}

// static
bool ParkableStringManager::ShouldPark(const StringImpl& string) {
  // Don't attempt to park strings smaller than this size.
  static constexpr unsigned int kSizeThreshold = 10000;
  // TODO(lizeb): Consider parking non-main thread strings.
  return string.length() > kSizeThreshold && IsMainThread();
}

scoped_refptr<ParkableStringImpl> ParkableStringManager::Add(
    scoped_refptr<StringImpl>&& string) {
  StringImpl* raw_ptr = string.get();
  auto it = unparked_strings_.find(raw_ptr);
  if (it != unparked_strings_.end())
    return it->value;

  auto new_parkable_string = base::MakeRefCounted<ParkableStringImpl>(
      std::move(string), ParkableStringImpl::ParkableState::kParkable);
  unparked_strings_.insert(raw_ptr, new_parkable_string.get());
  return new_parkable_string;
}

void ParkableStringManager::Remove(ParkableStringImpl* string,
                                   StringImpl* maybe_unparked_string) {
  DCHECK(IsMainThread());
  if (string->is_parked()) {
    auto it = parked_strings_.find(string);
    DCHECK(it != parked_strings_.end());
    parked_strings_.erase(it);
  } else {
    DCHECK(maybe_unparked_string);
    auto it = unparked_strings_.find(maybe_unparked_string);
    DCHECK(it != unparked_strings_.end());
    unparked_strings_.erase(it);
  }
}

void ParkableStringManager::OnParked(ParkableStringImpl* newly_parked_string,
                                     StringImpl* previous_unparked_string) {
  DCHECK(IsMainThread());
  DCHECK(newly_parked_string->is_parked());
  auto it = unparked_strings_.find(previous_unparked_string);
  DCHECK(it != unparked_strings_.end());
  unparked_strings_.erase(it);
  parked_strings_.insert(newly_parked_string);
}

void ParkableStringManager::OnUnparked(ParkableStringImpl* was_parked_string,
                                       StringImpl* new_unparked_string) {
  DCHECK(IsMainThread());
  DCHECK(!was_parked_string->is_parked());
  auto it = parked_strings_.find(was_parked_string);
  DCHECK(it != parked_strings_.end());
  parked_strings_.erase(it);
  unparked_strings_.insert(new_unparked_string, was_parked_string);
}

void ParkableStringManager::ParkAllIfRendererBackgrounded() {
  DCHECK(IsMainThread());
  if (!IsRendererBackgrounded())
    return;

  if (!base::FeatureList::IsEnabled(kCompressParkableStringsInBackground))
    return;

  size_t total_size = 0;
  for (ParkableStringImpl* str : parked_strings_)
    total_size += str->CharactersSizeInBytes();

  for (ParkableStringImpl* str : unparked_strings_.Values()) {
    str->Park();  // Parking is asynchronous.
    total_size += str->CharactersSizeInBytes();
  }

  size_t total_size_kb = total_size / 1000;
  UMA_HISTOGRAM_COUNTS_100000("Memory.MovableStringsTotalSizeKb",
                              total_size_kb);
  UMA_HISTOGRAM_COUNTS_1000("Memory.MovableStringsCount", Size());
}

size_t ParkableStringManager::Size() const {
  return parked_strings_.size() + unparked_strings_.size();
}

ParkableStringManager::ParkableStringManager()
    : backgrounded_(false), unparked_strings_(), parked_strings_() {}

}  // namespace blink
