// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/voice_isolation/buffered_voice_isolation.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"

namespace media {

namespace {

// Validates `component` before any member initializer dereferences it, and
// returns its frame size.
size_t GetValidatedInternalFrameSize(const VoiceIsolationComponent* component) {
  CHECK(component);
  const size_t frame_size = component->FrameSize();
  CHECK_GT(frame_size, 0u);
  CHECK_EQ(frame_size % BufferedVoiceIsolation::kNumBufferedFrames, 0u);
  CHECK_GT(component->FramesPerSecond(), 0u);
  return frame_size;
}

}  // namespace

BufferedVoiceIsolation::BufferedVoiceIsolation(
    std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation)
    : internal_voice_isolation_(std::move(internal_voice_isolation)),
      internal_input_(
          GetValidatedInternalFrameSize(internal_voice_isolation_.get())),
      internal_output_(internal_input_.size()),
      frame_size_(internal_input_.size() / kNumBufferedFrames),
      frames_per_second_(internal_voice_isolation_->FramesPerSecond() *
                         kNumBufferedFrames) {
  DVLOG(1) << "BufferedVoiceIsolation frame_size=" << FrameSize()
           << " internal_frame_size=" << internal_input_.size();
}

BufferedVoiceIsolation::~BufferedVoiceIsolation() = default;

void BufferedVoiceIsolation::ProcessAudio(base::span<const float> input,
                                          base::span<float> output) {
  const bool is_first_half = next_half_ == HalfFrame::kFirst;
  TRACE_EVENT("audio", "BufferedVoiceIsolation::ProcessAudio", "phase",
              is_first_half ? "buffer_first_half" : "process_full_frame");
  const size_t frame_size = FrameSize();
  CHECK_EQ(input.size(), frame_size);
  CHECK_EQ(output.size(), frame_size);

  // Store the new input in its half of the internal frame.
  const size_t input_offset = is_first_half ? 0 : frame_size;
  base::span(internal_input_)
      .subspan(input_offset, frame_size)
      .copy_from_nonoverlapping(input);

  // Once both halves are buffered, run the internal component on the full
  // frame.
  if (!is_first_half) {
    internal_voice_isolation_->ProcessAudio(internal_input_, internal_output_);
  }

  // Output the half that is one external frame behind the input: the second
  // half of the previous internal output, or the first half of the new one.
  const size_t output_offset = is_first_half ? frame_size : 0;
  output.copy_from_nonoverlapping(
      base::span(internal_output_).subspan(output_offset, frame_size));

  next_half_ = is_first_half ? HalfFrame::kSecond : HalfFrame::kFirst;
}

size_t BufferedVoiceIsolation::FrameSize() const {
  return frame_size_;
}

size_t BufferedVoiceIsolation::FramesPerSecond() const {
  return frames_per_second_;
}

void BufferedVoiceIsolation::ClearBuffers() {
  // Restart at the first half and drop buffered audio. std::fill does not
  // allocate, so this is safe on the real-time audio thread.
  next_half_ = HalfFrame::kFirst;
  std::fill(internal_input_.begin(), internal_input_.end(), 0.0f);
  std::fill(internal_output_.begin(), internal_output_.end(), 0.0f);
  internal_voice_isolation_->ClearBuffers();
}

base::TimeDelta BufferedVoiceIsolation::AlgorithmicDelay() const {
  // The output of each call is the input of the previous call, so the
  // buffering adds one external frame on top of the internal delay.
  return internal_voice_isolation_->AlgorithmicDelay() +
         base::Seconds(1) / FramesPerSecond();
}

}  // namespace media
