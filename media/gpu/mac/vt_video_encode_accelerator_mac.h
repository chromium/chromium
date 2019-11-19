// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_
#define MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_

#include <memory>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/base/mac/videotoolbox_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/webrtc/common_video/include/bitrate_adjuster.h"

namespace media {

// VideoToolbox.framework implementation of the VideoEncodeAccelerator
// interface for MacOSX. VideoToolbox makes no guarantees that it is thread
// safe, so this object is pinned to the thread on which it is constructed.
class MEDIA_GPU_EXPORT VTVideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  VTVideoEncodeAccelerator();
  ~VTVideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config, Client* client) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;

 private:
  // Holds the associated data of a video frame being processed.
  struct InProgressFrameEncode;

  // Holds output buffers coming from the encoder.
  struct EncodeOutput;

  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  // Encoding tasks to be run on |encoder_thread_|.
  void EncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);
  void RequestEncodingParametersChangeTask(uint32_t bitrate,
                                           uint32_t framerate);
  void DestroyTask();

  // Helper function to set bitrate.
  void SetAdjustedBitrate(int32_t bitrate);

  // Helper function to notify the client of an error on |client_task_runner_|.
  void NotifyError(VideoEncodeAccelerator::Error error);

  // Compression session callback function to handle compressed frames.
  static void CompressionCallback(void* encoder_opaque,
                                  void* request_opaque,
                                  OSStatus status,
                                  VTEncodeInfoFlags info,
                                  CMSampleBufferRef sbuf);
  void CompressionCallbackTask(OSStatus status,
                               std::unique_ptr<EncodeOutput> encode_output);

  // Copy CMSampleBuffer into a BitstreamBuffer and return it to the |client_|.
  void ReturnBitstreamBuffer(
      std::unique_ptr<EncodeOutput> encode_output,
      std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref);

  // Reset the encoder's compression session by destroying the existing one
  // using DestroyCompressionSession() and creating a new one. The new session
  // is configured using ConfigureCompressionSession().
  bool ResetCompressionSession();

  // Create a compression session.
  bool CreateCompressionSession(const gfx::Size& input_size);

  // Configure the current compression session using current encoder settings.
  bool ConfigureCompressionSession();

  // Destroy the current compression session if any. Blocks until all pending
  // frames have been flushed out (similar to EmitFrames without doing any
  // encoding work).
  void DestroyCompressionSession();

  base::ScopedCFTypeRef<VTCompressionSessionRef> compression_session_;

  gfx::Size input_visible_size_;
  size_t bitstream_buffer_size_;
  int32_t frame_rate_;
  int32_t initial_bitrate_;
  int32_t target_bitrate_;
  int32_t encoder_set_bitrate_;
  VideoCodecProfile h264_profile_;

  // Bitrate adjuster used to fix VideoToolbox's inconsistent bitrate issues.
  webrtc::BitrateAdjuster bitrate_adjuster_;

  // Bitstream buffers ready to be used to return encoded output as a FIFO.
  base::circular_deque<std::unique_ptr<BitstreamBufferRef>>
      bitstream_buffer_queue_;

  // EncodeOutput needs to be copied into a BitstreamBufferRef as a FIFO.
  base::circular_deque<std::unique_ptr<EncodeOutput>> encoder_output_queue_;

  // Our original calling task runner for the child thread.
  const scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to this object *MUST* be executed on
  // |client_task_runner_|.
  base::WeakPtr<Client> client_;
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

  // Thread checker to enforce that this object is used on a specific thread.
  // It is pinned on |client_task_runner_| thread.
  base::ThreadChecker thread_checker_;

  // This thread services tasks posted from the VEA API entry points by the
  // GPU child thread and CompressionCallback() posted from device thread.
  base::Thread encoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_thread_task_runner_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtr<VTVideoEncodeAccelerator> encoder_weak_ptr_;
  base::WeakPtrFactory<VTVideoEncodeAccelerator> encoder_task_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VTVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_
