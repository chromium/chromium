// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/jpeg_decode_accelerator.h"

// These data types are defined in va/va.h using typedef, reproduced here.
typedef struct _VAImageFormat VAImageFormat;
typedef unsigned int VASurfaceID;

namespace media {

class BitstreamBuffer;
struct JpegParseResult;
class UnalignedSharedMemory;
class VaapiWrapper;
class VaapiJpegDecodeAcceleratorTest;

// Alternative notation for the VA_FOURCC_YUY2 format, <va/va.h> doesn't provide
// this specific packing/ordering.
constexpr uint32_t VA_FOURCC_YUYV = 0x56595559;

// Class to provide JPEG decode acceleration for Intel systems with hardware
// support for it, and on which libva is available.
// Decoding tasks are performed in a separate decoding thread.
//
// Threading/life-cycle: this object is created & destroyed on the GPU
// ChildThread.  A few methods on it are called on the decoder thread which is
// stopped during |this->Destroy()|, so any tasks posted to the decoder thread
// can assume |*this| is still alive.  See |weak_this_| below for more details.
class MEDIA_GPU_EXPORT VaapiJpegDecodeAccelerator
    : public JpegDecodeAccelerator {
 public:
  VaapiJpegDecodeAccelerator(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~VaapiJpegDecodeAccelerator() override;

  // JpegDecodeAccelerator implementation.
  bool Initialize(JpegDecodeAccelerator::Client* client) override;
  void Decode(const BitstreamBuffer& bitstream_buffer,
              const scoped_refptr<VideoFrame>& video_frame) override;
  bool IsSupported() override;

 private:
  friend class VaapiJpegDecodeAcceleratorTest;

  // Notifies the client that an error has occurred and decoding cannot
  // continue. The client is notified on the |task_runner_|, i.e., the thread in
  // which |*this| was created.
  void NotifyError(int32_t bitstream_buffer_id, Error error);

  // Notifies the client that a decode is ready. The client is notified on the
  // |task_runner_|, i.e., the thread in which |*this| was created.
  void VideoFrameReady(int32_t bitstream_buffer_id);

  // Processes one decode request.
  void DecodeTask(int32_t bitstream_buffer_id,
                  std::unique_ptr<UnalignedSharedMemory> shm,
                  scoped_refptr<VideoFrame> video_frame);

  // Puts contents of |va_surface| into given |video_frame|, releases the
  // surface and passes the |input_buffer_id| of the resulting picture to
  // client for output.
  bool OutputPicture(VASurfaceID va_surface_id,
                     uint32_t va_surface_format,
                     int32_t input_buffer_id,
                     const scoped_refptr<VideoFrame>& video_frame);

  // Decodes a JPEG picture. It will fill VA-API parameters and call
  // corresponding VA-API methods according to the JPEG |parse_result|. Decoded
  // data will be outputted to the given |va_surface|. Returns false on failure.
  // |vaapi_wrapper| should be initialized in kDecode mode with
  // VAProfileJPEGBaseline profile. |va_surface| should be created with size at
  // least as large as the picture size.
  static bool DoDecode(VaapiWrapper* vaapi_wrapper,
                       const JpegParseResult& parse_result,
                       VASurfaceID va_surface);

  // ChildThread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // GPU IO task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The client of this class.
  Client* client_;

  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  // Comes after vaapi_wrapper_ to ensure its destructor is executed before
  // |vaapi_wrapper_| is destroyed.
  base::Thread decoder_thread_;
  // Use this to post tasks to |decoder_thread_| instead of
  // |decoder_thread_.task_runner()| because the latter will be NULL once
  // |decoder_thread_.Stop()| returns.
  scoped_refptr<base::SingleThreadTaskRunner> decoder_task_runner_;

  // The current VA surface for decoding.
  VASurfaceID va_surface_id_;
  // The coded size associated with |va_surface_id_|.
  gfx::Size coded_size_;
  // The VA RT format associated with |va_surface_id_|.
  unsigned int va_rt_format_;

  // WeakPtr factory for use in posting tasks from |decoder_task_runner_| back
  // to |task_runner_|.  Since |decoder_thread_| is a fully owned member of
  // this class, tasks posted to it may use base::Unretained(this), and tasks
  // posted from the |decoder_task_runner_| to |task_runner_| should use a
  // WeakPtr (obtained via weak_this_factory_.GetWeakPtr()).
  base::WeakPtrFactory<VaapiJpegDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiJpegDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODE_ACCELERATOR_H_
