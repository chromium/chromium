// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_CLIENT_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_collision_warner.h"
#include "media/capture/capture_export.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_device.h"

namespace media {
class VideoCaptureBufferPool;
class VideoFrameReceiver;
class VideoCaptureJpegDecoder;

using VideoCaptureJpegDecoderFactoryCB =
    base::OnceCallback<std::unique_ptr<VideoCaptureJpegDecoder>()>;

// Implementation of VideoCaptureDevice::Client that uses a buffer pool
// to provide buffers and converts incoming data to the I420 format for
// consumption by a given VideoFrameReceiver. If
// |optional_jpeg_decoder_factory_callback| is provided, the
// VideoCaptureDeviceClient will attempt to use it for decoding of MJPEG frames.
// Otherwise, it will use libyuv to perform MJPEG to I420 conversion in
// software.
//
// Methods of this class may be called from any thread, and in practice will
// often be called on some auxiliary thread depending on the platform and the
// device type; including, for example, the DirectShow thread on Windows, the
// v4l2_thread on Linux, and the UI thread for tab capture.
// The owner is responsible for making sure that the instance outlives these
// calls.
class CAPTURE_EXPORT VideoCaptureDeviceClient
    : public VideoCaptureDevice::Client {
 public:
#if defined(OS_CHROMEOS)
  VideoCaptureDeviceClient(
      VideoCaptureBufferType target_buffer_type,
      std::unique_ptr<VideoFrameReceiver> receiver,
      scoped_refptr<VideoCaptureBufferPool> buffer_pool,
      VideoCaptureJpegDecoderFactoryCB jpeg_decoder_factory_callback);
#else
  VideoCaptureDeviceClient(VideoCaptureBufferType target_buffer_type,
                           std::unique_ptr<VideoFrameReceiver> receiver,
                           scoped_refptr<VideoCaptureBufferPool> buffer_pool);
#endif  // defined(OS_CHROMEOS)
  ~VideoCaptureDeviceClient() override;

  static Buffer MakeBufferStruct(
      scoped_refptr<VideoCaptureBufferPool> buffer_pool,
      int buffer_id,
      int frame_feedback_id);

  // VideoCaptureDevice::Client implementation.
  // TODO(crbug.com/978143): remove |frame_feedback_id| default value.
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const VideoCaptureFormat& frame_format,
                              const gfx::ColorSpace& color_space,
                              int clockwise_rotation,
                              bool flip_y,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp,
                              int frame_feedback_id = 0) override;
  // TODO(crbug.com/978143): remove |frame_feedback_id| default value.
  void OnIncomingCapturedGfxBuffer(gfx::GpuMemoryBuffer* buffer,
                                   const VideoCaptureFormat& frame_format,
                                   int clockwise_rotation,
                                   base::TimeTicks reference_time,
                                   base::TimeDelta timestamp,
                                   int frame_feedback_id = 0) override;
  ReserveResult ReserveOutputBuffer(const gfx::Size& dimensions,
                                    VideoPixelFormat format,
                                    int frame_feedback_id,
                                    Buffer* buffer) override;
  void OnIncomingCapturedBuffer(Buffer buffer,
                                const VideoCaptureFormat& format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) override;
  void OnIncomingCapturedBufferExt(
      Buffer buffer,
      const VideoCaptureFormat& format,
      const gfx::ColorSpace& color_space,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      const VideoFrameMetadata& additional_metadata) override;
  void OnError(VideoCaptureError error,
               const base::Location& from_here,
               const std::string& reason) override;
  void OnFrameDropped(VideoCaptureFrameDropReason reason) override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  double GetBufferPoolUtilization() const override;

 private:
  // A branch of OnIncomingCapturedData for Y16 frame_format.pixel_format.
  void OnIncomingCapturedY16Data(const uint8_t* data,
                                 int length,
                                 const VideoCaptureFormat& frame_format,
                                 base::TimeTicks reference_time,
                                 base::TimeDelta timestamp,
                                 int frame_feedback_id);

  const VideoCaptureBufferType target_buffer_type_;

  // The receiver to which we post events.
  const std::unique_ptr<VideoFrameReceiver> receiver_;
  std::vector<int> buffer_ids_known_by_receiver_;

#if defined(OS_CHROMEOS)
  VideoCaptureJpegDecoderFactoryCB optional_jpeg_decoder_factory_callback_;
  std::unique_ptr<VideoCaptureJpegDecoder> external_jpeg_decoder_;
  base::OnceClosure on_started_using_gpu_cb_;
#endif  // defined(OS_CHROMEOS)

  // The pool of shared-memory buffers used for capturing.
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  VideoPixelFormat last_captured_pixel_format_;

  // Thread collision warner to ensure that producer-facing API is not called
  // concurrently. Producers are allowed to call from multiple threads, but not
  // concurrently.
  DFAKE_MUTEX(call_from_producer_);

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceClient);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_CLIENT_H_
