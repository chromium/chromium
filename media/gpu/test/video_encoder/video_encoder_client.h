// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_CLIENT_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/test/bitstream_helpers.h"
#include "media/gpu/test/video_encoder/video_encoder.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
namespace test {

class AlignedDataHelper;
class RawVideo;

// Video encoder client configuration.
// TODO(dstaessens): Add extra parameters (e.g. h264 output level)
struct VideoEncoderClientConfig {
  static constexpr uint32_t kDefaultBitrate = 200000;
  VideoEncoderClientConfig(
      const RawVideo* video,
      VideoCodecProfile output_profile,
      const std::vector<VideoEncodeAccelerator::Config::SpatialLayer>&
          spatial_layers,
      SVCInterLayerPredMode inter_layer_pred_mode,
      VideoEncodeAccelerator::Config::ContentType content_type,
      const media::VideoBitrateAllocation& bitrate,
      bool reverse);
  VideoEncoderClientConfig(const VideoEncoderClientConfig&);
  ~VideoEncoderClientConfig();

  // The output profile to be used.
  VideoCodecProfile output_profile = VideoCodecProfile::H264PROFILE_MAIN;
  // The resolution output by VideoEncoderClient.
  gfx::Size output_resolution;
  // The spatial layers for SVC stream, it's empty for simple stream.
  std::vector<VideoEncodeAccelerator::Config::SpatialLayer> spatial_layers;
  // The number of temporal/spatial layers and inter layer prediction of the
  // output stream.
  size_t num_temporal_layers = 1u;
  size_t num_spatial_layers = 1u;
  SVCInterLayerPredMode inter_layer_pred_mode = SVCInterLayerPredMode::kOff;
  VideoEncodeAccelerator::Config::ContentType content_type =
      VideoEncodeAccelerator::Config::ContentType::kCamera;
  // The maximum number of bitstream buffer encodes that can be requested
  // without waiting for the result of the previous encodes requests.
  size_t max_outstanding_encode_requests = 1;
  // The drop frame threshold. See VideoEncodeAccelerator::Config for detail.
  uint8_t drop_frame_thresh = 0;
  // The desired bitrate in bits/second.
  media::VideoBitrateAllocation bitrate_allocation;
  // The desired framerate in frames/second.
  uint32_t framerate = 30.0;
  // The interval of calling VideoEncodeAccelerator::Encode(). If this is
  // std::nullopt, Encode() is called once VideoEncodeAccelerator consumes
  // the previous VideoFrames.
  std::optional<base::TimeDelta> encode_interval = std::nullopt;
  // The number of frames to be encoded. This can be more than the number of
  // frames in the video, and in which case the VideoEncoderClient loops the
  // video during encoding.
  size_t num_frames_to_encode = 0;
  // The storage type of the input VideoFrames.
  VideoEncodeAccelerator::Config::StorageType input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kShmem;
  // True if the video should play backwards at reaching the end of video.
  // Otherwise the video loops. See the comment in AlignedDataHelper for detail.
  const bool reverse = false;
};

class VideoEncoderStats {
 public:
  VideoEncoderStats();
  VideoEncoderStats(const VideoEncoderStats&);
  ~VideoEncoderStats();
  VideoEncoderStats(uint32_t framerate,
                    size_t num_temporal_layers,
                    size_t num_spatial_layers);
  uint32_t Bitrate() const;
  uint32_t LayerBitrate(size_t spatial_idx, size_t temporal_idx) const;
  void Reset();

  uint32_t framerate = 0;
  size_t total_num_encoded_frames = 0;
  size_t total_encoded_frames_size = 0;
  size_t num_dropped_frames = 0;
  // Filled in spatial/temporal layer encoding and codec is vp9.
  std::vector<std::vector<size_t>> num_encoded_frames_per_layer;
  std::vector<std::vector<size_t>> encoded_frames_size_per_layer;
  size_t num_spatial_layers = 0;
  size_t num_temporal_layers = 0;
};

// The video encoder client is responsible for the communication between the
// test video encoder and the video encoder. It also communicates with the
// attached decoder buffer processors. The video encoder client can only have
// one active encoder at any time. To encode a different stream the Destroy()
// and Initialize() functions have to be called to destroy and re-create the
// encoder.
//
// All communication with the encoder is done on the |encoder_client_thread_|,
// so callbacks scheduled by the encoder can be executed asynchronously. This is
// necessary in order not to interrupt the test flow.
class VideoEncoderClient : public VideoEncodeAccelerator::Client {
 public:
  // Disallow copy and assign.
  VideoEncoderClient(const VideoEncoderClient&) = delete;
  VideoEncoderClient& operator=(const VideoEncoderClient&) = delete;

  ~VideoEncoderClient() override;

  // Return an instance of the VideoEncoderClient. The |event_cb| will be called
  // whenever an event occurs (e.g. frame encoded) and should be thread-safe.
  static std::unique_ptr<VideoEncoderClient> Create(
      const VideoEncoder::EventCallback& event_cb,
      std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors,
      const VideoEncoderClientConfig& config);

  // Initialize the video encode accelerator for the specified |video|.
  // Initialization is performed asynchronous, upon completion a 'kInitialized'
  // event will be sent to the test encoder.
  bool Initialize(const RawVideo* video);

  // Start encoding the video stream, encoder should be idle when this function
  // is called. This function is non-blocking, for each frame encoded a
  // 'kFrameEncoded' event will be sent to the test encoder.
  void Encode();
  // Flush all scheduled encode tasks. This function is non-blocking, a
  // kFlushing/kFlushDone event is sent upon start/finish. The kFlushDone
  // event is always sent after all associated kFrameEncoded events.
  void Flush();

  // Updates bitrate based on the specified |bitrate| and |framerate|.
  void UpdateBitrate(const VideoBitrateAllocation& bitrate, uint32_t framerate);

  // Force the next frame to be encoded to be a key frame.
  void ForceKeyFrame();

  // Wait until all bitstream processors have finished processing. Returns
  // whether processing was successful.
  bool WaitForBitstreamProcessors();

  // Get/Reset video encode statistics.
  VideoEncoderStats GetStats() const;
  void ResetStats();

  // VideoEncodeAccelerator::Client implementation
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) override;
  void NotifyErrorStatus(const EncoderStatus& status) override;
  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override;

 private:
  enum class VideoEncoderClientState {
    kUninitialized = 0,
    kIdle,
    kEncoding,
    kFlushing,
  };

  VideoEncoderClient(
      const VideoEncoder::EventCallback& event_cb,
      std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors,
      const VideoEncoderClientConfig& config);

  // Destroy the video encoder client.
  void Destroy();

  // Create a new video |encoder_| on the |encoder_client_thread_|.
  void CreateEncoderTask(const RawVideo* video,
                         bool* success,
                         base::WaitableEvent* done);
  // Destroy the active video |encoder_| on the |encoder_client_thread_|.
  void DestroyEncoderTask(base::WaitableEvent* done);

  // Start encoding video stream buffers on the |encoder_client_thread_|.
  void EncodeTask();
  // Instruct the encoder to encode the next video frame on the
  // |encoder_client_thread_|.
  void EncodeNextFrameTask();
  // Instruct the encoder to perform a flush on the |encoder_client_thread_|.
  void FlushTask();
  void UpdateBitrateTask(const VideoBitrateAllocation& bitrate,
                         uint32_t framerate);
  // Instruct the encoder to force a key frame on the |encoder_client_thread_|.
  void ForceKeyFrameTask();

  // Called by the encoder when a frame has been encoded.
  void EncodeDoneTask(base::TimeDelta timestamp);
  // Called by the encoder when flushing has completed.
  void FlushDoneTask(bool success);
  // Calls FlushDoneTask() if needed. This is necessary if Flush() flow is
  // simulated because VEA doesn't support Flush().
  void FlushDoneTaskIfNeeded();

  // Fire the specified event.
  void FireEvent(VideoEncoder::EncoderEvent event);

  // Create BitstreamRef from |buffer| and |metadata| passed to
  // |bitstream_processors_|.
  scoped_refptr<BitstreamProcessor::BitstreamRef> CreateBitstreamRef(
      int32_t bitstream_buffer_id,
      const BitstreamBufferMetadata& metadata);

  // Invoked when BitstreamBuffer associated with |bitstream_buffer_id| can be
  // reused by |encoder_|.
  void BitstreamBufferProcessed(int32_t bitstream_buffer_id);

  // Get the next bitstream buffer id to be used.
  int32_t GetNextBitstreamBufferId();

  // The callback used to notify the test video encoder of events.
  VideoEncoder::EventCallback event_cb_;
  // The list of bitstream processors. All bitstream buffers will be forwarded
  // to the bitstream processors (e.g. verification of contents).
  std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors_;

  // The currently active video encode accelerator.
  std::unique_ptr<media::VideoEncodeAccelerator> encoder_;
  // The video encoder client configuration.
  const VideoEncoderClientConfig encoder_client_config_;
  // The thread used for all communication with the video encode accelerator.
  base::Thread encoder_client_thread_;
  // The task runner associated with the |encoder_client_thread_|;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_client_task_runner_;

  // The current number of outgoing frame encode requests.
  size_t num_outstanding_encode_requests_ = 0;
  // Encoder client state, should only be accessed on the encoder client thread.
  VideoEncoderClientState encoder_client_state_;

  // The video being encoded, owned by the video encoder test environment.
  raw_ptr<const RawVideo> video_ = nullptr;
  // Helper used to align data and create frames from the raw video stream.
  std::unique_ptr<media::test::AlignedDataHelper> aligned_data_helper_;

  // Size of the output buffer requested by the encoder.
  size_t output_buffer_size_ = 0u;
  // Maps bitstream buffer Id's on the associated memory.
  std::map<int32_t, base::UnsafeSharedMemoryRegion> bitstream_buffers_;

  // Id to be used for the the next bitstream buffer.
  int32_t next_bitstream_buffer_id_ = 0;

  // A counter to how many VideoEncodeAccelerator::Encode() is called.
  size_t num_encodes_requested_ = 0;

  // A counter to track what frame is represented by a bitstream returned on
  // BitstreamBufferReady().
  size_t frame_index_ = 0;

  // A map from an input VideoFrame timestamp to the time when it is enqueued
  // into |encoder_|.
  std::map<base::TimeDelta, base::TimeTicks> source_timestamps_;

  // Force a key frame on next Encode(), only accessed on the
  // |encoder_client_thread_|.
  bool force_keyframe_ = false;

  VideoEncoderStats current_stats_ GUARDED_BY(stats_lock_);
  mutable base::Lock stats_lock_;

  SEQUENCE_CHECKER(test_sequence_checker_);
  SEQUENCE_CHECKER(encoder_client_sequence_checker_);

  base::WeakPtr<VideoEncoderClient> weak_this_;
  base::WeakPtrFactory<VideoEncoderClient> weak_this_factory_{this};
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_CLIENT_H_
