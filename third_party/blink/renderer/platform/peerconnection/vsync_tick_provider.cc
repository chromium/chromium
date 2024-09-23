// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/vsync_tick_provider.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/typed_macros.h"

namespace blink {

// static
scoped_refptr<VSyncTickProvider> VSyncTickProvider::Create(
    VSyncProvider& provider,
    scoped_refptr<base::SequencedTaskRunner> sequence,
    scoped_refptr<MetronomeSource::TickProvider> default_tick_provider) {
  scoped_refptr<VSyncTickProvider> tick_provider(new VSyncTickProvider(
      provider, sequence, std::move(default_tick_provider)));
  sequence->PostTask(FROM_HERE,
                     base::BindOnce(&VSyncTickProvider::Initialize,
                                    tick_provider->weak_factory_.GetWeakPtr()));
  return tick_provider;
}

VSyncTickProvider::VSyncTickProvider(
    VSyncProvider& vsync_provider,
    scoped_refptr<base::SequencedTaskRunner> sequence,
    scoped_refptr<MetronomeSource::TickProvider> default_tick_provider)
    : vsync_provider_(vsync_provider),
      sequence_(std::move(sequence)),
      default_tick_provider_(std::move(default_tick_provider)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VSyncTickProvider::~VSyncTickProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VSyncTickProvider::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  vsync_provider_->Initialize(base::BindPostTask(
      sequence_,
      base::BindRepeating(&VSyncTickProvider::OnTabVisibilityChange,
                          weak_factory_.GetWeakPtr()),
      FROM_HERE));
}

void VSyncTickProvider::RequestCallOnNextTick(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tick_callbacks_.push_back(std::move(callback));
  DCHECK_GT(tick_callbacks_.size(), 0u);
  if (state_ == State::kDrivenByVSync) {
    ScheduleVSync();
  } else {
    ScheduleDefaultTick();
  }
}

base::TimeDelta VSyncTickProvider::TickPeriod() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kDrivenByVSync) {
    return default_tick_provider_->TickPeriod();
  }
  return kVSyncTickPeriod;
}

void VSyncTickProvider::ScheduleVSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("webrtc", "ScheduleVSync");
  DCHECK_NE(state_, State::kDrivenByDefault);
  vsync_provider_->SetVSyncCallback(
      base::BindPostTask(sequence_,
                         base::BindOnce(&VSyncTickProvider::OnVSync,
                                        weak_tick_factory_.GetWeakPtr()),
                         FROM_HERE));
}

void VSyncTickProvider::ScheduleDefaultTick() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("webrtc", "ScheduleDefaultTick");
  default_tick_provider_->RequestCallOnNextTick(base::BindOnce(
      &VSyncTickProvider::OnDefaultTick, weak_tick_factory_.GetWeakPtr()));
}

void VSyncTickProvider::OnDefaultTick() {
  TRACE_EVENT0("webrtc", "OnDefaultTick");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeCalloutToClient();
}

void VSyncTickProvider::OnVSync() {
  TRACE_EVENT0("webrtc", "OnVSync");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kAwaitingVSync) {
    // Cancel old timer callbacks in flight.
    weak_tick_factory_.InvalidateWeakPtrs();
    state_ = State::kDrivenByVSync;
  }
  MaybeCalloutToClient();
}

void VSyncTickProvider::MaybeCalloutToClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WTF::Vector<base::OnceClosure> tick_callbacks;
  tick_callbacks.swap(tick_callbacks_);
  for (auto& tick_callback : tick_callbacks) {
    std::move(tick_callback).Run();
  }
}

void VSyncTickProvider::OnTabVisibilityChange(bool visible) {
  TRACE_EVENT0("webrtc", __func__);
  TRACE_EVENT_INSTANT1("webrtc", __func__, TRACE_EVENT_SCOPE_PROCESS, "visible",
                       visible);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kDrivenByDefault && visible) {
    state_ = State::kAwaitingVSync;
    ScheduleVSync();
  } else if (state_ != State::kDrivenByDefault && !visible) {
    // Schedule a new timer call and cancel old callbacks if driven by
    // vsyncs, since we're still driving default callbacks while we're
    // awaiting the first vsync.
    if (state_ == State::kDrivenByVSync) {
      weak_tick_factory_.InvalidateWeakPtrs();
      ScheduleDefaultTick();
    }
    state_ = State::kDrivenByDefault;
  }
}
}  // namespace blink
