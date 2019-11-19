// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_MJPEG_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_MJPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "components/chromeos_camera/mjpeg_decode_accelerator.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
class GpuMemoryBufferSupport;
}

namespace media {

class BitstreamBuffer;
class ScopedVAImage;
class UnalignedSharedMemory;
class VaapiWrapper;
class VideoFrame;

// Class to provide MJPEG decode acceleration for Intel systems with hardware
// support for it, and on which libva is available.
// Decoding tasks are performed on a separate |decoder_thread_|.
//
// Threading/life-cycle: this object is created & destroyed on the GPU
// ChildThread.  A few methods on it are called on the decoder thread which is
// stopped during |this->Destroy()|, so any tasks posted to the decoder thread
// can assume |*this| is still alive.  See |weak_this_| below for more details.
class MEDIA_GPU_EXPORT VaapiMjpegDecodeAccelerator
    : public chromeos_camera::MjpegDecodeAccelerator {
 public:
  VaapiMjpegDecodeAccelerator(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~VaapiMjpegDecodeAccelerator() override;

  // chromeos_camera::MjpegDecodeAccelerator implementation.
  bool Initialize(
      chromeos_camera::MjpegDecodeAccelerator::Client* client) override;
  void Decode(BitstreamBuffer bitstream_buffer,
              scoped_refptr<VideoFrame> video_frame) override;
  void Decode(int32_t task_id,
              base::ScopedFD src_dmabuf_fd,
              size_t src_size,
              off_t src_offset,
              scoped_refptr<VideoFrame> dst_frame) override;
  bool IsSupported() override;

 private:
  // Notifies the client that an error has occurred and decoding cannot
  // continue. The client is notified on the |task_runner_|, i.e., the thread in
  // which |*this| was created.
  void NotifyError(int32_t task_id, Error error);

  // Notifies the client that a decode is ready. The client is notified on the
  // |task_runner_|, i.e., the thread in which |*this| was created.
  void VideoFrameReady(int32_t task_id);

  // Processes one decode request.
  void DecodeFromShmTask(int32_t task_id,
                         std::unique_ptr<UnalignedSharedMemory> shm,
                         scoped_refptr<VideoFrame> dst_frame);
  void DecodeFromDmaBufTask(int32_t task_id,
                            base::ScopedFD src_dmabuf_fd,
                            size_t src_size,
                            off_t src_offset,
                            scoped_refptr<VideoFrame> dst_frame);

  // Decodes the JPEG in |src_image| into |dst_frame| and notifies the client
  // when finished or when an error occurs.
  void DecodeImpl(int32_t task_id,
                  base::span<const uint8_t> src_image,
                  scoped_refptr<VideoFrame> dst_frame);

  // Puts contents of |surface| into given |video_frame| using VA-API Video
  // Processing Pipeline (VPP), and passes the |input_buffer_id| of the
  // resulting picture to client for output.
  bool OutputPictureVppOnTaskRunner(const ScopedVASurface* surface,
                                    int32_t input_buffer_id,
                                    scoped_refptr<VideoFrame> video_frame);

  // Puts contents of |image| into given |video_frame| using libyuv, and passes
  // the |input_buffer_id| of the resulting picture to client for output.
  bool OutputPictureLibYuvOnTaskRunner(std::unique_ptr<ScopedVAImage> image,
                                       int32_t input_buffer_id,
                                       scoped_refptr<VideoFrame> video_frame);

  // ChildThread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // GPU IO task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The client of this class.
  chromeos_camera::MjpegDecodeAccelerator::Client* client_;

  VaapiJpegDecoder decoder_;

  // VaapiWrapper for VPP context. This is used to convert decoded data into
  // client buffer.
  scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper_;

  // For creating GpuMemoryBuffer from client DMA buffer that can be mapped for
  // software access.
  std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support_;

  base::Thread decoder_thread_;
  // Use this to post tasks to |decoder_thread_| instead of
  // |decoder_thread_.task_runner()| because the latter will be NULL once
  // |decoder_thread_.Stop()| returns.
  scoped_refptr<base::SingleThreadTaskRunner> decoder_task_runner_;

  // WeakPtr factory for use in posting tasks from |decoder_task_runner_| back
  // to |task_runner_|.  Since |decoder_thread_| is a fully owned member of
  // this class, tasks posted to it may use base::Unretained(this), and tasks
  // posted from the |decoder_task_runner_| to |task_runner_| should use a
  // WeakPtr (obtained via weak_this_factory_.GetWeakPtr()).
  base::WeakPtrFactory<VaapiMjpegDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiMjpegDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_MJPEG_DECODE_ACCELERATOR_H_
