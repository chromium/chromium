// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_JPEG_DECODER_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_JPEG_DECODER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator.h"
#include "gpu/config/gpu_info.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#include "media/capture/video/chromeos/video_capture_jpeg_decoder.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Implementation of media::VideoCaptureJpegDecoder that delegates to a
// chromeos_camera::mojom::MjpegDecodeAccelerator. When a frame is received in
// DecodeCapturedData(), it is copied to |in_shared_region_| for IPC transport
// to |decoder_|. When the decoder is finished with the frame, |decode_done_cb_|
// is invoked. Until |decode_done_cb_| is invoked, subsequent calls to
// DecodeCapturedData() are ignored.
// The given |decoder_task_runner| must allow blocking on |lock_|.
// Instances must be destroyed on |decoder_task_runner|, but the
// media::VideoCaptureJpegDecoder methods may be called from any thread.
class CAPTURE_EXPORT VideoCaptureJpegDecoderImpl
    : public VideoCaptureJpegDecoder,
      public chromeos_camera::MjpegDecodeAccelerator::Client {
 public:
  VideoCaptureJpegDecoderImpl(
      MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      DecodeDoneCB decode_done_cb,
      base::RepeatingCallback<void(const std::string&)> send_log_message_cb);
  ~VideoCaptureJpegDecoderImpl() override;

  // Implementation of VideoCaptureJpegDecoder:
  void Initialize() override;
  STATUS GetStatus() const override;
  void DecodeCapturedData(
      const uint8_t* data,
      size_t in_buffer_size,
      const media::VideoCaptureFormat& frame_format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      media::VideoCaptureDevice::Client::Buffer out_buffer) override;

  // chromeos_camera::MjpegDecodeAccelerator::Client implementation.
  // These will be called on |decoder_task_runner|.
  void VideoFrameReady(int32_t buffer_id) override;
  void NotifyError(
      int32_t buffer_id,
      chromeos_camera::MjpegDecodeAccelerator::Error error) override;

 private:
  void FinishInitialization();
  void OnInitializationDone(bool success);

  // Returns true if the decoding of last frame is not finished yet.
  bool IsDecoding_Locked() const;

  // Records |decoder_status_| to histogram.
  void RecordInitDecodeUMA_Locked();

  void DestroyDecoderOnIOThread(base::WaitableEvent* event);

  MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_;
  scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  // The underlying JPEG decode accelerator.
  std::unique_ptr<chromeos_camera::MjpegDecodeAccelerator> decoder_;

  // The callback to run when decode succeeds.
  const DecodeDoneCB decode_done_cb_;

  const base::RepeatingCallback<void(const std::string&)> send_log_message_cb_;
  bool has_received_decoded_frame_;

  // Guards |decode_done_closure_| and |decoder_status_|.
  mutable base::Lock lock_;

  // The closure of |decode_done_cb_| with bound parameters.
  base::OnceClosure decode_done_closure_;

  // Next id for input BitstreamBuffer.
  int32_t next_task_id_;

  // The id for current input BitstreamBuffer being decoded.
  int32_t task_id_;

  // Shared memory to store JPEG stream buffer. The input BitstreamBuffer is
  // backed by this.
  base::UnsafeSharedMemoryRegion in_shared_region_;
  base::WritableSharedMemoryMapping in_shared_mapping_;

  STATUS decoder_status_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoCaptureJpegDecoderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureJpegDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_JPEG_DECODER_IMPL_H_
