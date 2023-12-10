// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_JPEG_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_JPEG_ENCODE_ACCELERATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "components/chromeos_camera/jpeg_encode_accelerator.h"
#include "media/base/bitstream_buffer.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}  // namespace base

namespace media {

// Class to provide JPEG encode acceleration for Intel systems with hardware
// support for it, and on which libva is available.
// Encoding tasks are performed in a separate encoding thread.
//
// Threading/life-cycle: this object is created & destroyed on the GPU
// ChildThread.  Methods in nested class Encoder are called on the encoder
// thread which is stopped during destructor, so the callbacks bound with
// a weak this can be run on the encoder thread because it can assume
// VaapiJpegEncodeAccelerator is still alive.
class MEDIA_GPU_EXPORT VaapiJpegEncodeAccelerator
    : public chromeos_camera::JpegEncodeAccelerator {
 public:
  explicit VaapiJpegEncodeAccelerator(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  VaapiJpegEncodeAccelerator(const VaapiJpegEncodeAccelerator&) = delete;
  VaapiJpegEncodeAccelerator& operator=(const VaapiJpegEncodeAccelerator&) =
      delete;

  ~VaapiJpegEncodeAccelerator() override;

  // JpegEncodeAccelerator implementation.
  void InitializeAsync(
      chromeos_camera::JpegEncodeAccelerator::Client* client,
      chromeos_camera::JpegEncodeAccelerator::InitCB init_cb) override;
  size_t GetMaxCodedBufferSize(const gfx::Size& picture_size) override;

  // Currently only I420 format is supported for |video_frame|.
  void Encode(scoped_refptr<VideoFrame> video_frame,
              int quality,
              BitstreamBuffer* exif_buffer,
              BitstreamBuffer output_buffer) override;

  void EncodeWithDmaBuf(scoped_refptr<VideoFrame> input_frame,
                        scoped_refptr<VideoFrame> output_frame,
                        int quality,
                        int32_t task_id,
                        BitstreamBuffer* exif_buffer) override;

 private:
  // An input video frame and the corresponding output buffer awaiting
  // consumption, provided by the client.
  struct EncodeRequest {
    EncodeRequest(int32_t task_id,
                  scoped_refptr<VideoFrame> video_frame,
                  base::WritableSharedMemoryMapping exif_mapping,
                  base::WritableSharedMemoryMapping output_mapping,
                  int quality);

    EncodeRequest(const EncodeRequest&) = delete;
    EncodeRequest& operator=(const EncodeRequest&) = delete;

    ~EncodeRequest();

    int32_t task_id;
    scoped_refptr<VideoFrame> video_frame;
    base::WritableSharedMemoryMapping exif_mapping;
    base::WritableSharedMemoryMapping output_mapping;
    int quality;
  };

  // The Encoder class is a collection of methods that run on
  // |encoder_task_runner_|.
  class Encoder;

  // Notifies the client that an error has occurred and encoding cannot
  // continue.
  void NotifyError(int32_t task_id, Status status);

  void VideoFrameReady(int32_t task_id, size_t encoded_picture_size);

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The client of this class.
  raw_ptr<Client> client_ = nullptr;

  // The task runner on which the functions of |encoder_| are executed.
  scoped_refptr<base::SequencedTaskRunner> encoder_task_runner_;
  std::unique_ptr<Encoder> encoder_;

  // |weak_this_| is used to post tasks from |encoder_task_runner_| to
  // |task_runner_|.
  base::WeakPtr<VaapiJpegEncodeAccelerator> weak_this_;
  base::WeakPtrFactory<VaapiJpegEncodeAccelerator> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_JPEG_ENCODE_ACCELERATOR_H_
