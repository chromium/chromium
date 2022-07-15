// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_player/decoder_wrapper.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"

namespace media {
namespace test {

namespace {
// Get the name of the specified video player |event|.
const char* EventName(VideoPlayer::Event event) {
  switch (event) {
    case VideoPlayer::Event::kInitialized:
      return "Initialized";
    case VideoPlayer::Event::kFrameDecoded:
      return "FrameDecoded";
    case VideoPlayer::Event::kFlushing:
      return "Flushing";
    case VideoPlayer::Event::kFlushDone:
      return "FlushDone";
    case VideoPlayer::Event::kResetting:
      return "Resetting";
    case VideoPlayer::Event::kResetDone:
      return "ResetDone";
    case VideoPlayer::Event::kConfigInfo:
      return "ConfigInfo";
    default:
      return "Unknown";
  }
}
}  // namespace

VideoPlayer::~VideoPlayer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  decoder_wrapper_.reset();
}

// static
std::unique_ptr<VideoPlayer> VideoPlayer::Create(
    const DecoderWrapperConfig& config,
    std::unique_ptr<FrameRendererDummy> frame_renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors) {
  return base::WrapUnique(new VideoPlayer(config, std::move(frame_renderer),
                                          std::move(frame_processors)));
}

void VideoPlayer::SetEventWaitTimeout(base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  event_timeout_ = timeout;
}

bool VideoPlayer::Initialize(const Video* video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video);
  DVLOGF(4);

  decoder_wrapper_->Initialize(video);

  // Wait until the video decoder is initialized.
  if (!WaitForEvent(Event::kInitialized))
    return false;

  return true;
}

void VideoPlayer::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  // Play until the end of the video.
  PlayUntil(Event::kNumEvents);
}

void VideoPlayer::PlayUntil(Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  play_until_ = event;
  decoder_wrapper_->Play();
}

void VideoPlayer::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  decoder_wrapper_->Reset();
}

void VideoPlayer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  decoder_wrapper_->Flush();
}

bool VideoPlayer::WaitForEvent(Event sought_event, size_t times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(times, 1u);
  DVLOGF(4) << "Event: " << EventName(sought_event);

  base::TimeDelta time_waiting;
  base::AutoLock auto_lock(event_lock_);
  while (true) {
    while (!video_player_events_.empty()) {
      const auto received_event = video_player_events_.front();
      video_player_events_.pop();

      if (received_event == sought_event)
        times--;
      if (times == 0)
        return true;
    }

    // Check whether we've exceeded the maximum time we're allowed to wait.
    if (time_waiting >= event_timeout_) {
      LOG(ERROR) << "Timeout while waiting for '" << EventName(sought_event)
                 << "' event";
      return false;
    }

    const base::TimeTicks start_time = base::TimeTicks::Now();
    event_cv_.TimedWait(event_timeout_ - time_waiting);
    time_waiting += base::TimeTicks::Now() - start_time;
  }
}

bool VideoPlayer::WaitForFlushDone() {
  return WaitForEvent(Event::kFlushDone);
}

bool VideoPlayer::WaitForResetDone() {
  return WaitForEvent(Event::kResetDone);
}

bool VideoPlayer::WaitForFrameDecoded(size_t times) {
  return WaitForEvent(Event::kFrameDecoded, times);
}

size_t VideoPlayer::GetEventCount(Event event) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(event_lock_);
  return video_player_event_counts_[static_cast<size_t>(event)];
}

bool VideoPlayer::WaitForFrameProcessors() {
  return decoder_wrapper_->WaitForFrameProcessors();
}

void VideoPlayer::WaitForRenderer() {
  decoder_wrapper_->WaitForRenderer();
}

size_t VideoPlayer::GetFlushDoneCount() const {
  return GetEventCount(Event::kFlushDone);
}

size_t VideoPlayer::GetResetDoneCount() const {
  return GetEventCount(Event::kResetDone);
}

size_t VideoPlayer::GetFrameDecodedCount() const {
  return GetEventCount(Event::kFrameDecoded);
}

VideoPlayer::VideoPlayer(
    const DecoderWrapperConfig& config,
    std::unique_ptr<FrameRendererDummy> frame_renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors)
    : decoder_wrapper_(DecoderWrapper::Create(
          // base::Unretained is safe here because |decoder_wrapper_| is fully
          // owned.
          base::BindRepeating(&VideoPlayer::NotifyEvent,
                              base::Unretained(this)),
          std::move(frame_renderer),
          std::move(frame_processors),
          config)),
      event_cv_(&event_lock_) {}

bool VideoPlayer::NotifyEvent(Event event) {
  base::AutoLock auto_lock(event_lock_);
  video_player_events_.push(event);
  video_player_event_counts_[static_cast<size_t>(event)]++;
  event_cv_.Signal();

  const bool should_pause = play_until_.has_value() && play_until_ == event;
  return !should_pause;
}

}  // namespace test
}  // namespace media
