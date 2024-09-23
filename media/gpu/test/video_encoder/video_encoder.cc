// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/video_encoder/video_encoder.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/bitstream_helpers.h"
#include "media/gpu/test/raw_video.h"
#include "media/gpu/test/video_encoder/video_encoder_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {
// Get the name of the specified video encoder |event|.
const char* EventName(VideoEncoder::EncoderEvent event) {
  switch (event) {
    case VideoEncoder::EncoderEvent::kInitialized:
      return "Initialized";
    case VideoEncoder::EncoderEvent::kFrameReleased:
      return "FrameReleased";
    case VideoEncoder::EncoderEvent::kBitstreamReady:
      return "BitstreamReady";
    case VideoEncoder::EncoderEvent::kFlushing:
      return "Flushing";
    case VideoEncoder::EncoderEvent::kFlushDone:
      return "FlushDone";
    case VideoEncoder::EncoderEvent::kKeyFrame:
      return "KeyFrame";
    default:
      return "Unknown";
  }
}

// Default timeout used when waiting for events.
constexpr base::TimeDelta kDefaultEventWaitTimeout = base::Seconds(30);

// Default initial size used for |video_encoder_events_|.
constexpr size_t kDefaultEventListSize = 512;

constexpr std::pair<VideoEncoder::EncoderEvent, size_t> kInvalidEncodeUntil{
    VideoEncoder::kNumEvents, std::numeric_limits<size_t>::max()};
}  // namespace

// static
std::unique_ptr<VideoEncoder> VideoEncoder::Create(
    const VideoEncoderClientConfig& config,
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors) {
  auto video_encoder = base::WrapUnique(new VideoEncoder());
  if (!video_encoder->CreateEncoderClient(config,
                                          std::move(bitstream_processors))) {
    return nullptr;
  }
  return video_encoder;
}

VideoEncoder::VideoEncoder()
    : event_timeout_(kDefaultEventWaitTimeout),
      video_encoder_event_counts_{},
      next_unprocessed_event_(0),
      encode_until_(kInvalidEncodeUntil) {
  video_encoder_events_.reserve(kDefaultEventListSize);
}

VideoEncoder::~VideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  encoder_client_.reset();
}

bool VideoEncoder::CreateEncoderClient(
    const VideoEncoderClientConfig& config,
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(video_encoder_state_.load(), EncoderState::kUninitialized);
  DVLOGF(4);

  // base::Unretained is safe here as we will never receive callbacks after
  // destroying the video encoder, since the video encoder client will be
  // destroyed first.
  EventCallback event_cb =
      base::BindRepeating(&VideoEncoder::NotifyEvent, base::Unretained(this));

  encoder_client_ = VideoEncoderClient::Create(
      event_cb, std::move(bitstream_processors), config);
  if (!encoder_client_) {
    VLOGF(1) << "Failed to create video encoder client";
    return false;
  }

  return true;
}

void VideoEncoder::SetEventWaitTimeout(base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  event_timeout_ = timeout;
}

bool VideoEncoder::Initialize(const RawVideo* video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_encoder_state_ == EncoderState::kUninitialized ||
         video_encoder_state_ == EncoderState::kIdle);
  DCHECK(video);
  DVLOGF(4);

  if (!encoder_client_->Initialize(video))
    return false;

  // Wait until the video encoder is initialized.
  if (!WaitForEvent(EncoderEvent::kInitialized)) {
    LOG(ERROR) << "Timeout while initializing video encode accelerator";
    return false;
  }

  video_ = video;
  video_encoder_state_ = EncoderState::kIdle;
  return true;
}

void VideoEncoder::Encode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  // Encode until the end of the video.
  EncodeUntil(EncoderEvent::kNumEvents, std::numeric_limits<size_t>::max());
}

void VideoEncoder::EncodeUntil(EncoderEvent event, size_t event_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_encoder_state_.load() != EncoderState::kIdle) {
    LOG(ERROR) << "VideoEncoder state is not idle: "
               << static_cast<int>(video_encoder_state_.load());
    return;
  }
  DCHECK(encode_until_ == kInvalidEncodeUntil);
  DCHECK(video_);
  DVLOGF(4);

  // Start encoding the video.
  encode_until_ = std::make_pair(event, event_count);
  video_encoder_state_ = EncoderState::kEncoding;
  encoder_client_->Encode();
}

void VideoEncoder::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  encoder_client_->Flush();
}

void VideoEncoder::UpdateBitrate(const VideoBitrateAllocation& bitrate,
                                 uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  encoder_client_->UpdateBitrate(bitrate, framerate);
}

void VideoEncoder::ForceKeyFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  encoder_client_->ForceKeyFrame();
}

VideoEncoder::EncoderState VideoEncoder::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return video_encoder_state_;
}

bool VideoEncoder::WaitForEvent(EncoderEvent event, size_t times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4) << "Event ID: " << EventName(event);

  if (times == 0)
    return true;

  base::TimeDelta time_waiting;
  base::AutoLock auto_lock(event_lock_);
  while (true) {
    // Go through the list of events since last wait, looking for the event
    // we're interested in.
    while (next_unprocessed_event_ < video_encoder_events_.size()) {
      EncoderEvent cur_event = video_encoder_events_[next_unprocessed_event_++];
      if (cur_event == event) {
        if (--times == 0)
          return true;
      } else if (cur_event == EncoderEvent::kError) {
        LOG(ERROR) << "Got error event";
        return false;
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

bool VideoEncoder::WaitUntilIdle() {
  base::TimeDelta time_waiting;
  base::AutoLock auto_lock(event_lock_);
  while (true) {
    if (video_encoder_state_.load() == EncoderState::kIdle)
      return true;
    if (video_encoder_state_.load() == EncoderState::kError) {
      LOG(ERROR) << "Encoder in error state";
      return false;
    }

    // Check whether we've exceeded the maximum time we're allowed to wait.
    if (time_waiting >= event_timeout_) {
      LOG(ERROR) << "Timeout while waiting for EncodeUntil complete";
      return false;
    }

    const base::TimeTicks start_time = base::TimeTicks::Now();
    event_cv_.TimedWait(event_timeout_ - time_waiting);
    time_waiting += base::TimeTicks::Now() - start_time;
  }
}

bool VideoEncoder::WaitForFlushDone() {
  return WaitForEvent(EncoderEvent::kFlushDone);
}

bool VideoEncoder::WaitForFrameReleased(size_t times) {
  return WaitForEvent(EncoderEvent::kFrameReleased, times);
}

size_t VideoEncoder::GetEventCount(EncoderEvent event) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(event_lock_);
  return video_encoder_event_counts_[event];
}

bool VideoEncoder::WaitForBitstreamProcessors() {
  return !encoder_client_ || encoder_client_->WaitForBitstreamProcessors();
}

VideoEncoderStats VideoEncoder::GetStats() const {
  return !encoder_client_ ? VideoEncoderStats() : encoder_client_->GetStats();
}

void VideoEncoder::ResetStats() {
  if (encoder_client_)
    encoder_client_->ResetStats();
}

size_t VideoEncoder::GetFlushDoneCount() const {
  return GetEventCount(EncoderEvent::kFlushDone);
}

size_t VideoEncoder::GetFrameReleasedCount() const {
  return GetEventCount(EncoderEvent::kFrameReleased);
}

bool VideoEncoder::NotifyEvent(EncoderEvent event) {
  base::AutoLock auto_lock(event_lock_);
  if (event == EncoderEvent::kFlushDone)
    video_encoder_state_ = EncoderState::kIdle;
  else if (event == EncoderEvent::kError)
    video_encoder_state_ = EncoderState::kError;

  video_encoder_events_.push_back(event);
  video_encoder_event_counts_[event]++;

  bool should_continue_encoding = true;
  // Check whether video encoding should be paused after this event.
  if (encode_until_.first == event &&
      encode_until_.second == video_encoder_event_counts_[event]) {
    video_encoder_state_ = EncoderState::kIdle;
    encode_until_ = kInvalidEncodeUntil;
    should_continue_encoding = false;
  }
  event_cv_.Signal();
  return should_continue_encoding;
}
}  // namespace test
}  // namespace media
