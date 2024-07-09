// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/websockets/websocket_message_chunk_accumulator.h"

#include <string.h>
#include <algorithm>

#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"

namespace blink {

constexpr size_t WebSocketMessageChunkAccumulator::kSegmentSize;
constexpr base::TimeDelta WebSocketMessageChunkAccumulator::kFreeDelay;

WebSocketMessageChunkAccumulator::WebSocketMessageChunkAccumulator(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : timer_(std::move(task_runner),
             this,
             &WebSocketMessageChunkAccumulator::OnTimerFired) {}

WebSocketMessageChunkAccumulator::~WebSocketMessageChunkAccumulator() = default;

void WebSocketMessageChunkAccumulator::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  timer_.SetTaskRunnerForTesting(std::move(task_runner), tick_clock);
}

void WebSocketMessageChunkAccumulator::Append(base::span<const char> data) {
  if (!segments_.empty()) {
    const size_t to_be_written =
        std::min(data.size(), kSegmentSize - GetLastSegmentSize());
    base::ranges::copy(data.first(to_be_written),
                       segments_.back().get() + GetLastSegmentSize());
    data = data.subspan(to_be_written);
    size_ += to_be_written;
  }
  while (!data.empty()) {
    SegmentPtr segment_ptr;
    if (pool_.empty()) {
      segment_ptr = CreateSegment();
    } else {
      segment_ptr = std::move(pool_.back());
      pool_.pop_back();
    }
    const size_t to_be_written = std::min(data.size(), kSegmentSize);
    memcpy(segment_ptr.get(), data.data(), to_be_written);
    data = data.subspan(to_be_written);
    size_ += to_be_written;
    segments_.push_back(std::move(segment_ptr));
  }
}

Vector<base::span<const char>> WebSocketMessageChunkAccumulator::GetView()
    const {
  Vector<base::span<const char>> view;
  if (segments_.empty()) {
    return view;
  }

  view.reserve(segments_.size());
  for (wtf_size_t i = 0; i < segments_.size() - 1; ++i) {
    view.push_back(base::make_span(segments_[i].get(), kSegmentSize));
  }
  view.push_back(base::make_span(segments_.back().get(), GetLastSegmentSize()));
  return view;
}

void WebSocketMessageChunkAccumulator::Clear() {
  num_pooled_segments_to_be_removed_ =
      std::min(num_pooled_segments_to_be_removed_, pool_.size());
  size_ = 0;
  pool_.reserve(pool_.size() + segments_.size());
  for (auto& segment : segments_) {
    pool_.push_back(std::move(segment));
  }
  segments_.clear();

  if (timer_.IsActive()) {
    return;
  }

  // We will remove all the segments if no one uses them in the near future.
  num_pooled_segments_to_be_removed_ = pool_.size();
  if (num_pooled_segments_to_be_removed_ > 0) {
    timer_.StartOneShot(kFreeDelay, FROM_HERE);
  }
}

void WebSocketMessageChunkAccumulator::Reset() {
  segments_.clear();
  pool_.clear();
  size_ = 0;
  num_pooled_segments_to_be_removed_ = 0;
  timer_.Stop();
}

void WebSocketMessageChunkAccumulator::OnTimerFired(TimerBase*) {
  DCHECK(!timer_.IsActive());
  const auto to_be_removed =
      std::min(num_pooled_segments_to_be_removed_, pool_.size());
  pool_.EraseAt(pool_.size() - to_be_removed, to_be_removed);

  // We will remove all the segments if no one uses them in the near future.
  num_pooled_segments_to_be_removed_ = pool_.size();
  if (num_pooled_segments_to_be_removed_ > 0) {
    timer_.StartOneShot(kFreeDelay, FROM_HERE);
  }
}

void WebSocketMessageChunkAccumulator::Trace(Visitor* visitor) const {
  visitor->Trace(timer_);
}

}  // namespace blink
