// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_MJPEG_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_MJPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "components/chromeos_camera/mjpeg_decode_accelerator.h"
#include "media/gpu/chromeos/image_processor_backend.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}

namespace media {
class BitstreamBuffer;
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

  VaapiMjpegDecodeAccelerator(const VaapiMjpegDecodeAccelerator&) = delete;
  VaapiMjpegDecodeAccelerator& operator=(const VaapiMjpegDecodeAccelerator&) =
      delete;

  ~VaapiMjpegDecodeAccelerator() override;

  // chromeos_camera::MjpegDecodeAccelerator implementation.
  void InitializeAsync(
      chromeos_camera::MjpegDecodeAccelerator::Client* client,
      chromeos_camera::MjpegDecodeAccelerator::InitCB init_cb) override;
  void Decode(BitstreamBuffer bitstream_buffer,
              scoped_refptr<VideoFrame> video_frame) override;
  void Decode(int32_t task_id,
              base::ScopedFD src_dmabuf_fd,
              size_t src_size,
              off_t src_offset,
              scoped_refptr<VideoFrame> dst_frame) override;
  bool IsSupported() override;

 private:
  class Decoder;
  // Notifies the client that an error has occurred and decoding cannot
  // continue. The client is notified on the |task_runner_|, i.e., the thread in
  // which |*this| was created.
  void NotifyError(int32_t task_id, Error error);

  // Notifies the client that a decode is ready. The client is notified on the
  // |task_runner_|, i.e., the thread in which |*this| was created.
  void VideoFrameReady(int32_t task_id);

  // GPU IO task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The client of this class.
  raw_ptr<chromeos_camera::MjpegDecodeAccelerator::Client> client_ = nullptr;

  std::unique_ptr<Decoder> decoder_;

  // The task runner on which the functions of |decoder_| are executed.
  scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  // WeakPtr factory for use in posting tasks from |decoder_task_runner_| back
  // to |task_runner_|.  Since |decoder_thread_| is a fully owned member of
  // this class, tasks posted to it may use base::Unretained(this), and tasks
  // posted from the |decoder_task_runner_| to |task_runner_| should use a
  // WeakPtr (obtained via weak_this_factory_.GetWeakPtr()).
  base::WeakPtrFactory<VaapiMjpegDecodeAccelerator> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_MJPEG_DECODE_ACCELERATOR_H_
