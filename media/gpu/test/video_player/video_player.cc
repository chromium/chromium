// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "

namespace media {
namespace test {

namespace {
// Get the name of the specified video player |event|.
const char* EventName(VideoPlayerEvent event) {
  switch (event) {
    case VideoPlayerEvent::kInitialized:
      return "Initialized";
    case VideoPlayerEvent::kFrameDecoded:
      return "FrameDecoded";
    case VideoPlayerEvent::kFlushing:
      return "Flushing";
    case VideoPlayerEvent::kFlushDone:
      return "FlushDone";
    case VideoPlayerEvent::kResetting:
      return "Resetting";
    case VideoPlayerEvent::kResetDone:
      return "ResetDone";
    case VideoPlayerEvent::kConfigInfo:
      return "ConfigInfo";
    default:
      return "Unknown";
  }
}
}  // namespace

VideoPlayer::VideoPlayer()
    : video_(nullptr),
      video_player_state_(VideoPlayerState::kUninitialized),
      event_cv_(&event_lock_),
      video_player_event_counts_{},
      event_id_(0) {}

VideoPlayer::~VideoPlayer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  Destroy();
}

// static
std::unique_ptr<VideoPlayer> VideoPlayer::Create(
    const VideoDecoderClientConfig& config,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    std::unique_ptr<FrameRenderer> frame_renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors) {
  auto video_player = base::WrapUnique(new VideoPlayer());
  if (!video_player->CreateDecoderClient(config, gpu_memory_buffer_factory,
                                         std::move(frame_renderer),
                                         std::move(frame_processors))) {
    return nullptr;
  }
  return video_player;
}

bool VideoPlayer::CreateDecoderClient(
    const VideoDecoderClientConfig& config,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    std::unique_ptr<FrameRenderer> frame_renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(video_player_state_, VideoPlayerState::kUninitialized);
  DCHECK(frame_renderer);
  DVLOGF(4);

  // base::Unretained is safe here as we will never receive callbacks after
  // destroying the video player, since the video decoder client will be
  // destroyed first.
  EventCallback event_cb =
      base::BindRepeating(&VideoPlayer::NotifyEvent, base::Unretained(this));

  decoder_client_ = VideoDecoderClient::Create(
      event_cb, gpu_memory_buffer_factory, std::move(frame_renderer),
      std::move(frame_processors), config);
  if (!decoder_client_) {
    VLOGF(1) << "Failed to create video decoder client";
    return false;
  }

  return true;
}

void VideoPlayer::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(video_player_state_, VideoPlayerState::kDestroyed);
  DVLOGF(4);

  decoder_client_.reset();
  video_player_state_ = VideoPlayerState::kDestroyed;
}

void VideoPlayer::SetEventWaitTimeout(base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  event_timeout_ = timeout;
}

bool VideoPlayer::Initialize(const Video* video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_player_state_ == VideoPlayerState::kUninitialized ||
         video_player_state_ == VideoPlayerState::kIdle);
  DCHECK(video);
  DVLOGF(4);

  decoder_client_->Initialize(video);

  // Wait until the video decoder is initialized.
  if (!WaitForEvent(VideoPlayerEvent::kInitialized))
    return false;

  video_ = video;
  video_player_state_ = VideoPlayerState::kIdle;
  return true;
}

void VideoPlayer::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(video_player_state_, VideoPlayerState::kIdle);
  DVLOGF(4);

  // Play until the end of the video.
  PlayUntil(VideoPlayerEvent::kNumEvents, 0);
}

void VideoPlayer::PlayUntil(VideoPlayerEvent event, size_t event_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(video_player_state_, VideoPlayerState::kIdle);
  DCHECK(video_);
  DVLOGF(4);

  // Start decoding the video.
  play_until_ = std::make_pair(event, event_count);
  video_player_state_ = VideoPlayerState::kDecoding;
  decoder_client_->Play();
}

void VideoPlayer::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  decoder_client_->Reset();
}

void VideoPlayer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  decoder_client_->Flush();
}

base::TimeDelta VideoPlayer::GetCurrentTime() const {
  NOTIMPLEMENTED();
  return base::TimeDelta();
}

size_t VideoPlayer::GetCurrentFrame() const {
  NOTIMPLEMENTED();
  return 0;
}

VideoPlayerState VideoPlayer::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(event_lock_);
  return video_player_state_;
}

FrameRenderer* VideoPlayer::GetFrameRenderer() const {
  return decoder_client_->GetFrameRenderer();
}

bool VideoPlayer::WaitForEvent(VideoPlayerEvent event, size_t times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(times, 1u);
  DVLOGF(4) << "Event ID: " << static_cast<size_t>(event);

  base::TimeDelta time_waiting;
  base::AutoLock auto_lock(event_lock_);
  while (true) {
    // TODO(dstaessens@) Investigate whether we really need to keep the full
    // list of events for more complex testcases.
    // Go through list of events since last wait, looking for the event we're
    // interested in.
    for (; event_id_ < video_player_events_.size(); ++event_id_) {
      if (video_player_events_[event_id_] == event)
        times--;
      if (times == 0) {
        event_id_++;
        return true;
      }
    }

    // Check whether we've exceeded the maximum time we're allowed to wait.
    if (time_waiting >= event_timeout_) {
      LOG(ERROR) << "Timeout while waiting for '" << EventName(event)
                 << "' event";
      return false;
    }

    const base::TimeTicks start_time = base::TimeTicks::Now();
    event_cv_.TimedWait(event_timeout_ - time_waiting);
    time_waiting += base::TimeTicks::Now() - start_time;
  }
}

bool VideoPlayer::WaitForFlushDone() {
  return WaitForEvent(VideoPlayerEvent::kFlushDone);
}

bool VideoPlayer::WaitForResetDone() {
  return WaitForEvent(VideoPlayerEvent::kResetDone);
}

bool VideoPlayer::WaitForFrameDecoded(size_t times) {
  return WaitForEvent(VideoPlayerEvent::kFrameDecoded, times);
}

size_t VideoPlayer::GetEventCount(VideoPlayerEvent event) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(event_lock_);
  return video_player_event_counts_[static_cast<size_t>(event)];
}

bool VideoPlayer::WaitForFrameProcessors() {
  return !decoder_client_ || decoder_client_->WaitForFrameProcessors();
}

void VideoPlayer::WaitForRenderer() {
  if (decoder_client_)
    decoder_client_->WaitForRenderer();
}

size_t VideoPlayer::GetFlushDoneCount() const {
  return GetEventCount(VideoPlayerEvent::kFlushDone);
}

size_t VideoPlayer::GetResetDoneCount() const {
  return GetEventCount(VideoPlayerEvent::kResetDone);
}

size_t VideoPlayer::GetFrameDecodedCount() const {
  return GetEventCount(VideoPlayerEvent::kFrameDecoded);
}

bool VideoPlayer::NotifyEvent(VideoPlayerEvent event) {
  base::AutoLock auto_lock(event_lock_);
  if (event == VideoPlayerEvent::kFlushDone ||
      event == VideoPlayerEvent::kResetDone) {
    video_player_state_ = VideoPlayerState::kIdle;
  }

  video_player_events_.push_back(event);
  video_player_event_counts_[static_cast<size_t>(event)]++;
  event_cv_.Signal();

  // Check whether video playback should be paused after this event.
  if (play_until_.first == event &&
      play_until_.second ==
          video_player_event_counts_[static_cast<size_t>(event)]) {
    video_player_state_ = VideoPlayerState::kIdle;
    return false;
  }
  return true;
}

}  // namespace test
}  // namespace media
