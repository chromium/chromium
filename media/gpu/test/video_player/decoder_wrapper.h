// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_DECODER_WRAPPER_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_DECODER_WRAPPER_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/decoder_status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/test/video_player/decoder_listener.h"

namespace media {

class VideoFrame;

namespace test {

class VideoBitstream;
class EncodedDataHelper;
class FrameRendererDummy;
class VideoFrameProcessor;

// The supported video decoding implementation.
enum class DecoderImplementation {
  kVDA,    // VDA-based video decoder.
  kVD,     // VD-based video decoder.
  kVDVDA,  // VD-based video decoder with VdVDA.
};

// Video decoder wrapper configuration.
struct DecoderWrapperConfig {
  // The maximum number of bitstream buffer decodes that can be requested
  // without waiting for the result of the previous decode requests.
  size_t max_outstanding_decode_requests = 1;
  DecoderImplementation implementation = DecoderImplementation::kVDA;
  bool linear_output = false;
  // See VP9Decoder for information on this.
  bool ignore_resolution_changes_to_smaller_vp9 = false;
};

// This class wraps the VideoDecoder implementation and associated
// FrameRendererDummy and, maybe, VideoFrameProcessors. It maintains the
// communication between them, notifies |event_cb| of events and does all the
// necessary thread jumping between the parent thread and the dedicated worker
// thread.
class DecoderWrapper {
 public:
  DecoderWrapper(const DecoderWrapper&) = delete;
  DecoderWrapper& operator=(const DecoderWrapper&) = delete;

  ~DecoderWrapper();

  // Return an instance of the DecoderWrapper. The |event_cb| will be called
  // whenever an event occurs (e.g. frame decoded) and should be thread-safe.
  // The produced DecoderWrapper must be Initialize()d before being used.
  static std::unique_ptr<DecoderWrapper> Create(
      const DecoderListener::EventCallback& event_cb,
      std::unique_ptr<FrameRendererDummy> frame_renderer,
      std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
      const DecoderWrapperConfig& config);

  // Wait until all frame processors have finished processing. Returns whether
  // processing was successful.
  bool WaitForFrameProcessors();
  // Wait until the renderer has finished rendering all queued frames.
  void WaitForRenderer();

  // Initialize the video decoder for the specified |video|. This function can
  // be called multiple times and needs to be called before Play().
  // Initialization is performed asynchronous, upon completion a 'kInitialized'
  // event is thrown.
  void Initialize(const VideoBitstream* video);
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
  enum class DecoderWrapperState : size_t {
    kUninitialized = 0,
    kIdle,
    kDecoding,
    kFlushing,
    kResetting,
  };

  DecoderWrapper(
      const DecoderListener::EventCallback& event_cb,
      std::unique_ptr<FrameRendererDummy> renderer,
      std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
      const DecoderWrapperConfig& config);

  // All methods called ...Task() below are executed on |worker_task_runner_|.

  // Creates a new |decoder_|, returns whether creating was successful.
  void CreateDecoder();
  void CreateDecoderTask(base::WaitableEvent* done);
  void DestroyDecoderTask(base::WaitableEvent* done);

  // Methods below are the equivalent of the public homonym ones.
  void InitializeTask(const VideoBitstream* video, base::WaitableEvent* done);
  void PlayTask();
  void FlushTask();
  void ResetTask();

  // Instruct the decoder to decode the next video stream fragment on the
  // |worker_task_runner_|.
  void DecodeNextFragmentTask();

  // Callbacks for |decoder_|. See media::VideoDecoder.
  void OnDecoderInitializedTask(DecoderStatus status);
  void OnDecodeDoneTask(DecoderStatus status);
  void OnFrameReadyTask(scoped_refptr<VideoFrame> video_frame);
  void OnFlushDoneTask(DecoderStatus status);
  void OnResetDoneTask();

  // Called by the decoder when a resolution change was requested, returns
  // whether we should continue or abort the resolution change.
  bool OnResolutionChangedTask();

  // Fires the specified event, and returns true if the caller should continue
  // decoding.
  bool FireEvent(DecoderListener::Event event);

  SEQUENCE_CHECKER(parent_sequence_checker_);
  SEQUENCE_CHECKER(worker_sequence_checker_);

  DecoderListener::EventCallback event_cb_;
  std::unique_ptr<FrameRendererDummy> frame_renderer_;
  std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors_;

  std::unique_ptr<media::VideoDecoder> decoder_
      GUARDED_BY_CONTEXT(worker_sequence_checker_);
  const DecoderWrapperConfig decoder_wrapper_config_;

  const scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  DecoderWrapperState state_ GUARDED_BY_CONTEXT(worker_sequence_checker_);

  // Decoded video frame index.
  size_t current_frame_index_ = 0;
  // The current number of decode requests in |decoder_|, for DCHECK purposes.
  // Increased in DecodeNextFragmentTask() and decreased in OnDecodeDoneTask().
  size_t num_outstanding_decode_requests_ = 0;

  // TODO(dstaessens@) Replace with StreamParser.
  std::unique_ptr<media::test::EncodedDataHelper> encoded_data_helper_;

  // These two are latched on Initialize() to be able to query HasConfigInfo()
  // during DecodeNextFragmentTask().
  VideoCodec input_video_codec_;
  VideoCodecProfile input_video_profile_;

  base::WeakPtr<DecoderWrapper> weak_this_;
  base::WeakPtrFactory<DecoderWrapper> weak_this_factory_{this};
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_DECODER_WRAPPER_H_
