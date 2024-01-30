// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_JPEG_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_JPEG_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "components/chromeos_camera/jpeg_encode_accelerator.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/parsers/jpeg_parser.h"

namespace {

// Input pixel format V4L2_PIX_FMT_YUV420M has 3 physical planes.
constexpr size_t kMaxI420Plane = 3;
constexpr size_t kMaxNV12Plane = 2;

// Output pixel format V4L2_PIX_FMT_JPEG(_RAW) has only one physical plane.
constexpr size_t kMaxJpegPlane = 1;

// This class can only handle V4L2_PIX_FMT_YUV420(M) as input, so
// kMaxI420Plane can only be 3.
static_assert(kMaxI420Plane == 3,
              "kMaxI420Plane must be 3 as input may be V4L2_PIX_FMT_YUV420M");
// This class can only handle V4L2_PIX_FMT_JPEG(_RAW) as output, so
// kMaxJpegPlanes can only be 1.
static_assert(
    kMaxJpegPlane == 1,
    "kMaxJpegPlane must be 1 as output must be V4L2_PIX_FMT_JPEG(_RAW)");
}  // namespace

namespace base {
class WaitableEvent;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

class MEDIA_GPU_EXPORT V4L2JpegEncodeAccelerator
    : public chromeos_camera::JpegEncodeAccelerator {
 public:
  V4L2JpegEncodeAccelerator(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  V4L2JpegEncodeAccelerator(const V4L2JpegEncodeAccelerator&) = delete;
  V4L2JpegEncodeAccelerator& operator=(const V4L2JpegEncodeAccelerator&) =
      delete;

  ~V4L2JpegEncodeAccelerator() override;

  // JpegEncodeAccelerator implementation.
  void InitializeAsync(
      chromeos_camera::JpegEncodeAccelerator::Client* client,
      chromeos_camera::JpegEncodeAccelerator::InitCB init_cb) override;
  size_t GetMaxCodedBufferSize(const gfx::Size& picture_size) override;
  void Encode(scoped_refptr<media::VideoFrame> video_frame,
              int quality,
              BitstreamBuffer* exif_buffer,
              BitstreamBuffer output_buffer) override;

  void EncodeWithDmaBuf(scoped_refptr<VideoFrame> input_frame,
                        scoped_refptr<VideoFrame> output_frame,
                        int quality,
                        int32_t task_id,
                        BitstreamBuffer* exif_buffer) override;

 private:
  void InitializeTask(chromeos_camera::JpegEncodeAccelerator::Client* client,
                      InitCB init_cb);

  // Record for input buffers.
  struct I420BufferRecord {
    I420BufferRecord();
    ~I420BufferRecord();
    void* address[kMaxI420Plane];  // mmap() address.
    size_t length[kMaxI420Plane];  // mmap() length.

    // Set true during QBUF and DQBUF. |address| will be accessed by hardware.
    bool at_device;
  };

  // Record for output buffers.
  struct JpegBufferRecord {
    JpegBufferRecord();
    ~JpegBufferRecord();
    void* address[kMaxJpegPlane];  // mmap() address.
    size_t length[kMaxJpegPlane];  // mmap() length.

    // Set true during QBUF and DQBUF. |address| will be accessed by hardware.
    bool at_device;
  };

  // Job record. Jobs are processed in a FIFO order. This is separated from
  // I420BufferRecord, because a I420BufferRecord of input may be returned
  // before we dequeue the corresponding output buffer. It can't always be
  // associated with a JpegBufferRecord of output immediately either, because at
  // the time of submission we may not have one available (and don't need one
  // to submit input to the device).
  struct JobRecord {
    JobRecord(scoped_refptr<VideoFrame> input_frame,
              scoped_refptr<VideoFrame> output_frame,
              int32_t task_id,
              int quality,
              base::WritableSharedMemoryMapping exif_mapping);
    JobRecord(scoped_refptr<VideoFrame> input_frame,
              int quality,
              int32_t task_id,
              base::WritableSharedMemoryMapping exif_mapping,
              base::WritableSharedMemoryMapping output_mapping);
    ~JobRecord();

    // Input frame buffer.
    scoped_refptr<VideoFrame> input_frame;

    // Output frame buffer.
    scoped_refptr<VideoFrame> output_frame;

    // JPEG encode quality.
    int quality;

    // Encode task ID.
    int32_t task_id;
    // Memory mapped from |output_buffer|.
    base::WritableSharedMemoryMapping output_mapping;

    // Memory mapped from |exif_buffer|.
    // It contains EXIF data to be inserted into JPEG image. If `IsValid()` is
    // false, the JFIF APP0 segment will be inserted.
    base::WritableSharedMemoryMapping exif_mapping;
  };

  // Encode Instance. One EncodedInstanceDmaBuf is used for a specific set of
  // jpeg parameters. The stored parameters are jpeg quality and resolutions of
  // input image. We execute all EncodedInstanceDmaBuf methods on
  // |encoder_task_runner_|.
  class EncodedInstanceDmaBuf {
   public:
    EncodedInstanceDmaBuf(V4L2JpegEncodeAccelerator* parent);
    ~EncodedInstanceDmaBuf();

    bool Initialize();

    // Create V4L2 buffers for input and output.
    bool CreateBuffers(gfx::Size input_coded_size,
                       const VideoFrameLayout& input_layout,
                       size_t output_buffer_size);

    // Set up JPEG related parameters in V4L2 device.
    bool SetUpJpegParameters(int quality, gfx::Size coded_size);

    // Dequeue last frame and enqueue next frame.
    void ServiceDevice();

    // Destroy input and output buffers.
    void DestroyTask();

    base::queue<std::unique_ptr<JobRecord>> input_job_queue_;
    base::queue<std::unique_ptr<JobRecord>> running_job_queue_;

   private:
    // Combined the encoded data from |output_frame| with the JFIF/EXIF data.
    // Add JPEG Marks if needed. Add EXIF section by |exif_shm|.
    size_t FinalizeJpegImage(scoped_refptr<VideoFrame> output_frame,
                             size_t buffer_size,
                             base::WritableSharedMemoryMapping exif_mapping);

    bool SetInputBufferFormat(gfx::Size coded_size,
                              const VideoFrameLayout& input_layout);
    bool SetOutputBufferFormat(gfx::Size coded_size, size_t buffer_size);
    bool RequestInputBuffers();
    bool RequestOutputBuffers();

    void EnqueueInput();
    void EnqueueOutput();
    void Dequeue();
    bool EnqueueInputRecord();
    bool EnqueueOutputRecord();

    void DestroyInputBuffers();
    void DestroyOutputBuffers();

    // Return the number of input/output buffers enqueued to the device.
    size_t InputBufferQueuedCount();
    size_t OutputBufferQueuedCount();

    void NotifyError(int32_t task_id, Status status);

    // The number of input buffers and output buffers.
    const size_t kBufferCount = 2;

    // Pointer back to the parent.
    V4L2JpegEncodeAccelerator* parent_;

    // Layout that represents the input data.
    std::optional<VideoFrameLayout> device_input_layout_;

    // The V4L2Device this class is operating upon.
    scoped_refptr<V4L2Device> device_;

    std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support_;

    // Input queue state.
    bool input_streamon_;
    // Indices of input buffers ready to use; LIFO since we don't care about
    // ordering.
    std::vector<int> free_input_buffers_;

    // Output queue state.
    bool output_streamon_;
    // Indices of output buffers ready to use; LIFO since we don't care about
    // ordering.
    std::vector<int> free_output_buffers_;

    // Pixel format of input buffer.
    uint32_t input_buffer_pixelformat_;

    // Number of physical planes the input buffers have.
    size_t input_buffer_num_planes_;

    // Pixel format of output buffer.
    uint32_t output_buffer_pixelformat_;

    // sizeimage of output buffer.
    uint32_t output_buffer_sizeimage_;
  };

  void VideoFrameReady(int32_t task_id, size_t encoded_picture_size);
  void NotifyError(int32_t task_id, Status status);

  // Enqueue the incoming frame.
  void EncodeTask(std::unique_ptr<JobRecord> job_record);

  // Trigger ServiceDevice of EncodedInstanceDmaBuf class.
  void ServiceDeviceTask();

  // Destroy input and output buffers.
  void DestroyTask(base::WaitableEvent* waiter);

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The client of this class.
  chromeos_camera::JpegEncodeAccelerator::Client* client_;

  // Encode task runner.
  scoped_refptr<base::SequencedTaskRunner> encoder_task_runner_;

  // All the below members except |weak_factory_| are accessed on
  // |encoder_task_runner_| only (if it's running).

  // The |latest_input_buffer_coded_size_| and |latest_quality_| are used to
  // check if we need to open new EncodedInstanceDmaBuf.
  // Latest coded size of input buffer.
  gfx::Size latest_input_buffer_coded_size_
      GUARDED_BY_CONTEXT(encoder_sequence_);
  // Latest encode quality.
  int latest_quality_ GUARDED_BY_CONTEXT(encoder_sequence_);
  // JEA may open multiple devices for different input parameters.
  // We handle the |encoded_instances_dma_buf_| by order for keeping user's
  // input order.
  std::queue<std::unique_ptr<EncodedInstanceDmaBuf>> encoded_instances_dma_buf_
      GUARDED_BY_CONTEXT(encoder_sequence_);

  SEQUENCE_CHECKER(encoder_sequence_);

  // Point to |this| for use in posting tasks to |encoder_task_runner_|.
  // |weak_ptr_for_encoder_| is required, even though we synchronously destroy
  // variables on |encoder_task_runner_| in destructor, because a task can be
  // posted to |encoder_task_runner_| within DestroyTask().
  base::WeakPtr<V4L2JpegEncodeAccelerator> weak_ptr_for_encoder_;
  base::WeakPtrFactory<V4L2JpegEncodeAccelerator> weak_factory_for_encoder_;

  // Point to |this| for use in posting tasks from the encoder thread back to
  // |io_taask_runner_|.
  base::WeakPtr<V4L2JpegEncodeAccelerator> weak_ptr_;
  base::WeakPtrFactory<V4L2JpegEncodeAccelerator> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_JPEG_ENCODE_ACCELERATOR_H_
