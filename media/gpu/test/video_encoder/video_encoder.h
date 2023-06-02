// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_H_

#include <limits.h>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace media {
class VideoBitrateAllocation;

namespace test {

class BitstreamProcessor;
class RawVideo;
class VideoEncoderClient;
struct VideoEncoderClientConfig;
class VideoEncoderStats;

// This class provides a framework to build video encode accelerator tests upon.
// It provides methods to control video encoding, and wait for specific events
// to occur.
class VideoEncoder {
 public:
  // Different video encoder states.
  enum class EncoderState { kUninitialized = 0, kIdle, kEncoding, kError };

  // The list of events that can be thrown by the video encoder.
  enum EncoderEvent {
    kInitialized,
    kFrameReleased,
    kBitstreamReady,
    kFlushing,
    kFlushDone,
    kKeyFrame,
    kError,
    kNumEvents,
  };

  using EventCallback = base::RepeatingCallback<bool(EncoderEvent)>;

  // Create an instance of the video encoder.
  // TODO(hiroh): Take raw pointers of bitstream_processors so that they are
  // destroyed on the same sequence where they are created.
  static std::unique_ptr<VideoEncoder> Create(
      const VideoEncoderClientConfig& config,
      std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors =
          {});

  // Disallow copy and assign.
  VideoEncoder(const VideoEncoder&) = delete;
  VideoEncoder& operator=(const VideoEncoder&) = delete;

  ~VideoEncoder();

  // Wait until all processors have finished processing the currently queued
  // list of bitstream buffers. Returns whether processing was successful.
  bool WaitForBitstreamProcessors();

  // Get/Reset video encode statistics.
  VideoEncoderStats GetStats() const;
  void ResetStats();

  // Set the maximum time we will wait for an event to finish.
  void SetEventWaitTimeout(base::TimeDelta timeout);

  // Initialize the video encoder for the specified |video|. The |video| will
  // not be owned by the video encoder, the caller should guarantee it outlives
  // the video encoder.
  bool Initialize(const RawVideo* video);
  // Start encoding the video asynchronously.
  void Encode();
  // Encode the video asynchronously. Automatically pause encoding when the
  // specified |event| occurred |event_count| times.
  void EncodeUntil(EncoderEvent event, size_t event_count = 1);
  // Flush the encoder.
  void Flush();
  // Updates bitrate based on the specified |bitrate| and |framerate|.
  void UpdateBitrate(const VideoBitrateAllocation& bitrate, uint32_t framerate);
  // Force key frame.
  void ForceKeyFrame();

  // Get the current state of the video encoder.
  EncoderState GetState() const;

  // Wait for an event to occur the specified number of times. All events that
  // occurred since last calling this function will be taken into account. All
  // events with different types will be consumed. Will return false if the
  // specified timeout is exceeded while waiting for the events.
  bool WaitForEvent(EncoderEvent event, size_t times = 1);
  // Wait until the |video_encoder_state_| becomes kIdle.
  bool WaitUntilIdle();
  // Helper function to wait for a FlushDone event.
  bool WaitForFlushDone();
  // Helper function to wait for the specified number of FrameReleased events.
  bool WaitForFrameReleased(size_t times);

  // Get the number of times the specified event occurred.
  size_t GetEventCount(EncoderEvent event) const;
  // Helper function to get the number of FlushDone events thrown.
  size_t GetFlushDoneCount() const;
  // Helper function to get the number of FrameReleased events thrown.
  size_t GetFrameReleasedCount() const;

 private:
  VideoEncoder();

  bool CreateEncoderClient(
      const VideoEncoderClientConfig& config,
      std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors);

  // Notify the video encoder an event has occurred (e.g. bitstream ready).
  // Returns whether the encoder client should continue encoding frames.
  bool NotifyEvent(EncoderEvent event);

  // The video currently being encoded.
  raw_ptr<const RawVideo> video_ = nullptr;
  // The state of the video encoder.
  std::atomic<EncoderState> video_encoder_state_{EncoderState::kUninitialized};
  // The video encoder client communicating between this class and the hardware
  // video encode accelerator.
  std::unique_ptr<VideoEncoderClient> encoder_client_;

  // The timeout used when waiting for events.
  base::TimeDelta event_timeout_;

  mutable base::Lock event_lock_;
  base::ConditionVariable event_cv_{&event_lock_};
  // The list of events thrown by the video encoder client.
  std::vector<EncoderEvent> video_encoder_events_ GUARDED_BY(event_lock_);
  // The number of times each event has occurred.
  size_t video_encoder_event_counts_[EncoderEvent::kNumEvents] GUARDED_BY(
      event_lock_);
  // The index of the next event to start at, when waiting for events.
  size_t next_unprocessed_event_ GUARDED_BY(event_lock_);

  // Automatically pause encoding once the video encoder has seen the specified
  // number of events occur.
  std::pair<EncoderEvent, size_t> encode_until_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_H_
