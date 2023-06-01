// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/test/raw_video.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace test {

// Helper class allowing one thread to wait on a notification from another.
// If notifications come in faster than they are Wait()'d for, they are
// accumulated (so exactly as many Wait() calls will unblock as Notify() calls
// were made, regardless of order).
template <typename StateEnum>
class ClientStateNotification {
 public:
  ClientStateNotification();
  ~ClientStateNotification();

  // Used to notify a single waiter of a ClientState.
  void Notify(StateEnum state);
  // Used by waiters to wait for the next ClientState Notification.
  StateEnum Wait();

 private:
  base::Lock lock_;
  base::ConditionVariable cv_;
  base::queue<StateEnum> pending_states_for_notification_;
};

template <typename StateEnum>
ClientStateNotification<StateEnum>::ClientStateNotification() : cv_(&lock_) {}

template <typename StateEnum>
ClientStateNotification<StateEnum>::~ClientStateNotification() {}

template <typename StateEnum>
void ClientStateNotification<StateEnum>::Notify(StateEnum state) {
  base::AutoLock auto_lock(lock_);
  pending_states_for_notification_.push(state);
  cv_.Signal();
}

template <typename StateEnum>
StateEnum ClientStateNotification<StateEnum>::Wait() {
  base::AutoLock auto_lock(lock_);
  while (pending_states_for_notification_.empty())
    cv_.Wait();
  StateEnum ret = pending_states_for_notification_.front();
  pending_states_for_notification_.pop();
  return ret;
}

struct IvfFrame {
  IvfFrameHeader header;
  raw_ptr<uint8_t> data = nullptr;
};

// Read functions to fill IVF file header and IVF frame header from |data|.
// |data| must have sufficient length.
IvfFileHeader GetIvfFileHeader(const base::span<const uint8_t>& data);
IvfFrameHeader GetIvfFrameHeader(const base::span<const uint8_t>& data);

// The helper class to save data as ivf format.
class IvfWriter {
 public:
  IvfWriter(base::FilePath output_filepath);
  bool WriteFileHeader(VideoCodec codec,
                       const gfx::Size& resolution,
                       uint32_t frame_rate,
                       uint32_t num_frames);
  bool WriteFrame(uint32_t data_size, uint64_t timestamp, const uint8_t* data);

 private:
  base::File output_file_;
};

// Helper to extract fragments from encoded video stream.
class EncodedDataHelper {
 public:
  EncodedDataHelper(base::span<const uint8_t> stream, VideoCodec codec);
  ~EncodedDataHelper();

  // Compute and return the next fragment to be sent to the decoder, starting
  // from the current position in the stream, and advance the current position
  // to after the returned fragment.
  scoped_refptr<DecoderBuffer> GetNextBuffer();
  static bool HasConfigInfo(const uint8_t* data,
                            size_t size,
                            VideoCodecProfile profile);

  void Rewind() { next_pos_to_decode_ = 0; }
  bool AtHeadOfStream() const { return next_pos_to_decode_ == 0; }
  bool ReachEndOfStream() const { return next_pos_to_decode_ == data_.size(); }

  size_t num_skipped_fragments() { return num_skipped_fragments_; }

 private:
  // For h.264/HEVC.
  scoped_refptr<DecoderBuffer> GetNextFragment();
  // For VP8/9.
  scoped_refptr<DecoderBuffer> GetNextFrame();
  absl::optional<IvfFrameHeader> GetNextIvfFrameHeader() const;
  absl::optional<IvfFrame> ReadNextIvfFrame();

  // Helpers for GetBytesForNextFragment above.
  size_t GetBytesForNextNALU(size_t pos);
  bool IsNALHeader(const std::string& data, size_t pos);
  bool LookForSPS(size_t* skipped_fragments_count);

  std::string data_;
  const VideoCodec codec_;
  size_t next_pos_to_decode_ = 0;
  size_t num_skipped_fragments_ = 0;
};

#if defined(ARCH_CPU_ARM_FAMILY)
// ARM performs CPU cache management with CPU cache line granularity. We thus
// need to ensure our buffers are CPU cache line-aligned (64 byte-aligned).
// Otherwise newer kernels will refuse to accept them, and on older kernels
// we'll be treating ourselves to random corruption.
// Moreover, some hardware codecs require 128-byte alignment for physical
// buffers.
constexpr size_t kPlatformBufferAlignment = 128;
#else
constexpr size_t kPlatformBufferAlignment = 8;
#endif

// Helper to align data and extract frames from raw video streams.
// GetNextFrame() returns VideoFrames with a specified |storage_type|. The
// VideoFrames are aligned by the specified |alignment| in the case of
// MojoSharedBuffer VideoFrame. On the other hand, GpuMemoryBuffer based
// VideoFrame is determined by the GpuMemoryBuffer allocation backend.
// GetNextFrame() returns valid frame if AtEndOfStream() returns false, i.e.,
// until GetNextFrame() is called |num_read_frames| times.
// |num_frames| is the number of frames contained in |stream|. |num_read_frames|
// can be larger than |num_frames|.
// If |reverse| is true , GetNextFrame() for a frame returns frames in a
// round-trip playback fashion (0, 1,.., |num_frames| - 2, |num_frames| - 1,
// |num_frames| - 1, |num_frames_| - 2,.., 1, 0, 0, 1,..) so that the content of
// returned frames is consecutive.
// If |reverse| is false, GetNextFrame() just loops the stream (0, 1,..,
// |num_frames| - 2, |num_frames| - 1, 0, 1,..), so the content of returned
// frames is not consecutive.
class AlignedDataHelper {
 public:
  AlignedDataHelper(const RawVideo* video,
                    uint32_t num_read_frames,
                    bool reverse,
                    const gfx::Size& aligned_coded_size,
                    const gfx::Size& natural_size,
                    uint32_t frame_rate,
                    VideoFrame::StorageType storage_type);
  ~AlignedDataHelper();

  // Compute and return the next frame to be sent to the encoder.
  scoped_refptr<VideoFrame> GetNextFrame();

  // Rewind to the position of the video stream.
  void Rewind();
  // Check whether we are at the start of the video stream.
  bool AtHeadOfStream() const;
  // Check whether we are at the end of the video stream.
  bool AtEndOfStream() const;
  // Change the timing between frames.
  void UpdateFrameRate(uint32_t frame_rate);

 private:
  struct VideoFrameData;
  enum class CreateFrameMode {
    kAllAtOnce,
    kOnDemand,
  };
  scoped_refptr<VideoFrame> CreateVideoFrameFromVideoFrameData(
      const VideoFrameData& video_frame_data,
      base::TimeDelta frame_timestamp) const;

  static VideoFrameData CreateVideoFrameData(
      VideoFrame::StorageType storage_type,
      const RawVideo::FrameData& src_frame,
      const VideoFrameLayout& src_layout,
      const VideoFrameLayout& dst_layout);

  const raw_ptr<const RawVideo> video_;
  // The number of frames in the given |stream|.
  const uint32_t num_frames_;
  // The number of frames to be read. It may be more than |num_frames_|.
  const uint32_t num_read_frames_;

  const bool reverse_;

  const CreateFrameMode create_frame_mode_;

  // The index of VideoFrame to be read next.
  uint32_t frame_index_ = 0;

  const VideoFrame::StorageType storage_type_;

  // The layout of VideoFrames returned by GetNextFrame().
  absl::optional<VideoFrameLayout> layout_;
  const gfx::Rect visible_rect_;
  const gfx::Size natural_size_;

  base::TimeDelta time_stamp_interval_;
  base::TimeDelta elapsed_frame_time_;

  // The frame data returned by GetNextFrame().
  std::vector<VideoFrameData> video_frame_data_;
};

// Small helper class to extract video frames from raw data streams.
// However, the data wrapped by VideoFrame is not guaranteed to be aligned.
// This class doesn't change |video|, but cannot be mark it as constant because
// GetFrame() returns non const |data_| wrapped by the returned VideoFrame.
// If |reverse| is true , GetNextFrame() for a frame returns frames in a
// round-trip playback fashion (0, 1,.., |num_frames| - 2, |num_frames| - 1,
// |num_frames| - 1, |num_frames_| - 2,.., 1, 0, 0, 1,..).
// If |reverse| is false, GetNextFrame() just loops the stream (0, 1,..,
// |num_frames| - 2, |num_frames| - 1, 0, 1,..).
class RawDataHelper {
 public:
  RawDataHelper(const RawVideo* video, bool reverse);
  ~RawDataHelper();

  // Returns i-th VideoFrame in |video|. The returned frame doesn't own the
  // underlying video data.
  scoped_refptr<const VideoFrame> GetFrame(size_t index) const;

 private:
  // |video| and its associated data must outlive this class and VideoFrames
  // returned by GetFrame().
  const raw_ptr<const RawVideo> video_;

  const bool reverse_;
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_TEST_HELPERS_H_
