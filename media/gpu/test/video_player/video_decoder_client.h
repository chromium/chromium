// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/decode_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/test/video_player/video_player.h"

namespace media {

class Video;
class VideoFrame;

namespace test {

class EncodedDataHelper;
class FrameRenderer;
class VideoFrameProcessor;

// TODO(dstaessens@) Remove allocation mode, temporary added here so we can
// support the thumbnail test for older platforms that don't support import.
enum class AllocationMode {
  kImport,    // Client allocates video frame memory.
  kAllocate,  // Video decoder allocates video frame memory.
};

// Video decoder client configuration.
struct VideoDecoderClientConfig {
  // The maximum number of bitstream buffer decodes that can be requested
  // without waiting for the result of the previous decode requests.
  size_t max_outstanding_decode_requests = 1;
  // How the pictures buffers should be allocated.
  AllocationMode allocation_mode = AllocationMode::kImport;
  // Use VD-based video decoders instead of VDA-based video decoders.
  bool use_vd = false;
};

// The video decoder client is responsible for the communication between the
// video player and the video decoder. It also communicates with the frame
// renderer and other components. The video decoder client can only have one
// active decoder at any time. To decode a different stream the DestroyDecoder()
// and CreateDecoder() functions have to be called to destroy and re-create the
// decoder.
//
// All communication with the decoder is done on the |decoder_client_thread_|,
// so callbacks scheduled by the decoder can be executed asynchronously. This is
// necessary if we don't want to interrupt the test flow.
class VideoDecoderClient {
 public:
  ~VideoDecoderClient();

  // Return an instance of the VideoDecoderClient. The
  // |gpu_memory_buffer_factory|, |frame_renderer| and |frame_processors| will
  // not be owned by the decoder client, the caller should guarantee they
  // outlive the decoder client. The |event_cb| will be called whenever an event
  // occurs (e.g. frame decoded) and should be thread-safe. Initialization is
  // performed asynchronous, upon completion a 'kInitialized' event will be
  // thrown.
  static std::unique_ptr<VideoDecoderClient> Create(
      const VideoPlayer::EventCallback& event_cb,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      std::unique_ptr<FrameRenderer> frame_renderer,
      std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
      const VideoDecoderClientConfig& config);

  // Wait until all frame processors have finished processing. Returns whether
  // processing was successful.
  bool WaitForFrameProcessors();
  // Wait until the renderer has finished rendering all queued frames.
  void WaitForRenderer();
  // Get the frame renderer associated with the video decoder client.
  FrameRenderer* GetFrameRenderer() const;

  // Initialize the video decoder for the specified |video|. This function can
  // be called multiple times and needs to be called before Play().
  // Initialization is performed asynchronous, upon completion a 'kInitialized'
  // event is thrown.
  void Initialize(const Video* video);
  // Start decoding the video stream, decoder should be idle when this function
  // is called. This function is non-blocking, for each frame decoded a
  // 'kFrameDecoded' event will be thrown.
  void Play();
  // Queue decoder flush. This function is non-blocking, a kFlushing/kFlushDone
  // event is thrown upon start/finish.
  void Flush();
  // Queue decoder reset. This function is non-blocking, a kResetting/kResetDone
  // event is thrown upon start/finish.
  void Reset();

 private:
  enum class VideoDecoderClientState : size_t {
    kUninitialized = 0,
    kIdle,
    kDecoding,
    kFlushing,
    kResetting,
  };

  VideoDecoderClient(
      const VideoPlayer::EventCallback& event_cb,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      std::unique_ptr<FrameRenderer> renderer,
      std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
      const VideoDecoderClientConfig& config);

  // Create a new decoder, returns whether creating was successful.
  bool CreateDecoder();
  // Destroy the currently active decoder.
  void DestroyDecoder();

  // Create a new video |decoder_| on the |decoder_client_thread_|.
  void CreateDecoderTask(bool* success, base::WaitableEvent* done);
  // Destroy the active video |decoder_| on the |decoder_client_thread_|.
  void DestroyDecoderTask(base::WaitableEvent* done);
  // Initialize the video |decoder_| with |video| on the
  // |decoder_client_thread_|.
  void InitializeDecoderTask(const Video* video, base::WaitableEvent* done);

  // Start decoding video stream fragments on the |decoder_client_thread_|.
  void PlayTask();
  // Instruct the decoder to decode the next video stream fragment on the
  // |decoder_client_thread_|.
  void DecodeNextFragmentTask();
  // Instruct the decoder to perform a flush on the |decoder_client_thread_|.
  void FlushTask();
  // Instruct the decoder to perform a Reset on the |decoder_client_thread_|.
  void ResetTask();

  // The below functions are callbacks provided to the video decoder. They are
  // all executed on the |decoder_client_thread_|.
  // Called by the decoder when initialization has completed.
  void DecoderInitializedTask(bool status);
  // Called by the decoder when a fragment has been decoded.
  void DecodeDoneTask(media::DecodeStatus status);
  // Called by the decoder when a video frame is ready.
  void FrameReadyTask(scoped_refptr<VideoFrame> video_frame);
  // Called by the decoder when flushing has completed.
  void FlushDoneTask(media::DecodeStatus status);
  // Called by the decoder when resetting has completed.
  void ResetDoneTask();

  // Fire the specified event.
  void FireEvent(VideoPlayerEvent event);

  VideoPlayer::EventCallback event_cb_;
  std::unique_ptr<FrameRenderer> frame_renderer_;
  std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors_;

  std::unique_ptr<media::VideoDecoder> decoder_;
  const VideoDecoderClientConfig decoder_client_config_;
  base::Thread decoder_client_thread_;

  // Decoder client state, should only be accessed on the decoder client thread.
  VideoDecoderClientState decoder_client_state_;

  // Index of the frame that's currently being decoded.
  size_t current_frame_index_ = 0;
  // The current number of outgoing bitstream buffers decode requests.
  size_t num_outstanding_decode_requests_ = 0;

  // TODO(dstaessens@) Replace with StreamParser.
  std::unique_ptr<media::test::EncodedDataHelper> encoded_data_helper_;
  // The video being decoded.
  const Video* video_ = nullptr;

  // Owned by VideoPlayerTestEnvironment.
  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;

  SEQUENCE_CHECKER(video_player_sequence_checker_);
  SEQUENCE_CHECKER(decoder_client_sequence_checker_);

  base::WeakPtr<VideoDecoderClient> weak_this_;
  base::WeakPtrFactory<VideoDecoderClient> weak_this_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderClient);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_
