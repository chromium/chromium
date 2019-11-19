// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_JPEG_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_JPEG_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/chromeos_camera/jpeg_encode_accelerator.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_jpeg_encode_accelerator.h"
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

namespace media {

class MEDIA_GPU_EXPORT V4L2JpegEncodeAccelerator
    : public chromeos_camera::JpegEncodeAccelerator {
 public:
  V4L2JpegEncodeAccelerator(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~V4L2JpegEncodeAccelerator() override;

  // JpegEncodeAccelerator implementation.
  chromeos_camera::JpegEncodeAccelerator::Status Initialize(
      chromeos_camera::JpegEncodeAccelerator::Client* client) override;
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
              BitstreamBuffer* exif_buffer);
    JobRecord(scoped_refptr<VideoFrame> input_frame,
              int quality,
              BitstreamBuffer* exif_buffer,
              BitstreamBuffer output_buffer);
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
    UnalignedSharedMemory output_shm;
    // Offset used for |output_shm|.
    off_t output_offset;

    // Memory mapped from |exif_buffer|.
    // It contains EXIF data to be inserted into JPEG image. If it's nullptr,
    // the JFIF APP0 segment will be inserted.
    std::unique_ptr<UnalignedSharedMemory> exif_shm;
    // Offset used for |exif_shm|.
    off_t exif_offset;
  };

  // TODO(wtlee): To be deprecated. (crbug.com/944705)
  //
  // Encode Instance. One EncodedInstance is used for a specific set of jpeg
  // parameters. The stored parameters are jpeg quality and resolutions of input
  // image.
  // We execute all EncodedInstance methods on |encoder_task_runner_| except
  // Initialize().
  class EncodedInstance {
   public:
    EncodedInstance(V4L2JpegEncodeAccelerator* parent);
    ~EncodedInstance();

    bool Initialize();

    // Create V4L2 buffers for input and output.
    bool CreateBuffers(gfx::Size input_coded_size, size_t output_buffer_size);

    // Set up JPEG related parameters in V4L2 device.
    bool SetUpJpegParameters(int quality, gfx::Size coded_size);

    // Dequeue last frame and enqueue next frame.
    void ServiceDevice();

    // Destroy input and output buffers.
    void DestroyTask();

    base::queue<std::unique_ptr<JobRecord>> input_job_queue_;
    base::queue<std::unique_ptr<JobRecord>> running_job_queue_;

   private:
    // Prepare full JPEG markers except SOI and EXIF/APP0 markers in
    // |jpeg_markers_|.
    void PrepareJpegMarkers(gfx::Size coded_size);

    // Copy the encoded data from |output_buffer| to the |dst_ptr| provided by
    // the client. Add JPEG Marks if needed. Add EXIF section by |exif_shm|.
    size_t FinalizeJpegImage(uint8_t* dst_ptr,
                             const JpegBufferRecord& output_buffer,
                             size_t buffer_size,
                             std::unique_ptr<UnalignedSharedMemory> exif_shm);

    bool SetInputBufferFormat(gfx::Size coded_size);
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

    // Fill the quantization table into |dst_table|. The value is scaled by
    // the |quality| and |basic_table|.
    // We use the the Independent JPEG Group's formula to scale scale table.
    // http://www.ijg.org/
    static void FillQuantizationTable(int quality,
                                      const uint8_t* basic_table,
                                      uint8_t* dst_table);

    // The number of input buffers and output buffers.
    const size_t kBufferCount = 2;

    // Pointer back to the parent.
    V4L2JpegEncodeAccelerator* parent_;

    // The V4L2Device this class is operating upon.
    scoped_refptr<V4L2Device> device_;

    // Input queue state.
    bool input_streamon_;
    // Mapping of int index to an input buffer record.
    std::vector<I420BufferRecord> input_buffer_map_;
    // Indices of input buffers ready to use; LIFO since we don't care about
    // ordering.
    std::vector<int> free_input_buffers_;

    // Output queue state.
    bool output_streamon_;
    // Mapping of int index to an output buffer record.
    std::vector<JpegBufferRecord> output_buffer_map_;
    // Indices of output buffers ready to use; LIFO since we don't care about
    // ordering.
    std::vector<int> free_output_buffers_;

    // Pixel format of input buffer.
    uint32_t input_buffer_pixelformat_;

    // Number of physical planes the input buffers have.
    size_t input_buffer_num_planes_;

    // Pixel format of output buffer.
    uint32_t output_buffer_pixelformat_;

    // Height of input buffer returned by driver.
    uint32_t input_buffer_height_;

    // Bytes per line for each plane.
    uint32_t bytes_per_line_[kMaxI420Plane];

    // JPEG Quantization table for V4L2_PIX_FMT_JPEG_RAW.
    JpegQuantizationTable quantization_table_[2];

    // JPEG markers for V4L2_PIX_FMT_JPEG_RAW.
    // We prepare markers in the EncodedInstance setup stage, and reuse it for
    // every encoding.
    std::vector<uint8_t> jpeg_markers_;
  };

  // Encode Instance. One EncodedInstance is used for a specific set of jpeg
  // parameters. The stored parameters are jpeg quality and resolutions of input
  // image.
  // We execute all EncodedInstance methods on |encoder_task_runner_| except
  // Initialize().
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
    // Prepare full JPEG markers except SOI and EXIF/APP0 markers in
    // |jpeg_markers_|.
    void PrepareJpegMarkers(gfx::Size coded_size);

    // Combined the encoded data from |output_frame| with the JFIF/EXIF data.
    // Add JPEG Marks if needed. Add EXIF section by |exif_shm|.
    size_t FinalizeJpegImage(scoped_refptr<VideoFrame> output_frame,
                             size_t buffer_size,
                             std::unique_ptr<UnalignedSharedMemory> exif_shm);

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

    // Fill the quantization table into |dst_table|. The value is scaled by
    // the |quality| and |basic_table|.
    // We use the the Independent JPEG Group's formula to scale scale table.
    // http://www.ijg.org/
    static void FillQuantizationTable(int quality,
                                      const uint8_t* basic_table,
                                      uint8_t* dst_table);

    // The number of input buffers and output buffers.
    const size_t kBufferCount = 2;

    // Pointer back to the parent.
    V4L2JpegEncodeAccelerator* parent_;

    // Layout that represents the input data.
    base::Optional<VideoFrameLayout> device_input_layout_;

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

    // Height of input buffer returned by driver.
    uint32_t input_buffer_height_;

    // JPEG Quantization table for V4L2_PIX_FMT_JPEG_RAW.
    JpegQuantizationTable quantization_table_[2];

    // JPEG markers for V4L2_PIX_FMT_JPEG_RAW.
    // We prepare markers in the EncodedInstance setup stage, and reuse it for
    // every encoding.
    std::vector<uint8_t> jpeg_markers_;
  };

  void VideoFrameReady(int32_t task_id, size_t encoded_picture_size);
  void NotifyError(int32_t task_id, Status status);

  // Run on |encoder_thread_| to enqueue the incoming frame.
  void EncodeTask(std::unique_ptr<JobRecord> job_record);
  // TODO(wtlee): To be deprecated. (crbug.com/944705)
  void EncodeTaskLegacy(std::unique_ptr<JobRecord> job_record);

  // Run on |encoder_thread_| to trigger ServiceDevice of EncodedInstance class.
  void ServiceDeviceTask();
  // TODO(wtlee): To be deprecated. (crbug.com/944705)
  void ServiceDeviceTaskLegacy();

  // Run on |encoder_thread_| to destroy input and output buffers.
  void DestroyTask();

  // The |latest_input_buffer_coded_size_| and |latest_quality_| are used to
  // check if we need to open new EncodedInstance.

  // Latest coded size of input buffer.
  gfx::Size latest_input_buffer_coded_size_;
  // TODO(wtlee): To be deprecated. (crbug.com/944705)
  gfx::Size latest_input_buffer_coded_size_legacy_;

  // Latest encode quality.
  int latest_quality_;
  // TODO(wtlee): To be deprecated. (crbug.com/944705)
  int latest_quality_legacy_;

  // ChildThread's task runner.
  scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The client of this class.
  chromeos_camera::JpegEncodeAccelerator::Client* client_;

  // Thread to communicate with the device.
  base::Thread encoder_thread_;
  // Encode task runner.
  scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;

  // All the below members except |weak_factory_| are accessed from
  // |encoder_thread_| only (if it's running).

  // JEA may open multiple devices for different input parameters.
  // We handle the |encoded_instances_| by order for keeping user's input order.
  std::queue<std::unique_ptr<EncodedInstance>> encoded_instances_;
  std::queue<std::unique_ptr<EncodedInstanceDmaBuf>> encoded_instances_dma_buf_;

  // Point to |this| for use in posting tasks from the encoder thread back to
  // the ChildThread.
  base::WeakPtr<V4L2JpegEncodeAccelerator> weak_ptr_;
  // Weak factory for producing weak pointers on the child thread.
  base::WeakPtrFactory<V4L2JpegEncodeAccelerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2JpegEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_JPEG_ENCODE_ACCELERATOR_H_
