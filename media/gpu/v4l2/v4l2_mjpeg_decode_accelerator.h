// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_MJPEG_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_MJPEG_DECODE_ACCELERATOR_H_

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
#include "components/chromeos_camera/mjpeg_decode_accelerator.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

class VideoFrame;

class MEDIA_GPU_EXPORT V4L2MjpegDecodeAccelerator
    : public chromeos_camera::MjpegDecodeAccelerator {
 public:
  // Job record. Jobs are processed in a FIFO order. This is separate from
  // BufferRecord of input, because a BufferRecord of input may be returned
  // before we dequeue the corresponding output buffer. It can't always be
  // associated with a BufferRecord of output immediately either, because at
  // the time of submission we may not have one available (and don't need one
  // to submit input to the device).
  class JobRecord;

  V4L2MjpegDecodeAccelerator(
      const scoped_refptr<V4L2Device>& device,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~V4L2MjpegDecodeAccelerator() override;

  // MjpegDecodeAccelerator implementation.
  bool Initialize(
      chromeos_camera::MjpegDecodeAccelerator::Client* client) override;
  void Decode(BitstreamBuffer bitstream_buffer,
              scoped_refptr<VideoFrame> video_frame) override;
  void Decode(int32_t task_id,
              base::ScopedFD src_dmabuf_fd,
              size_t src_size,
              off_t src_offset,
              scoped_refptr<media::VideoFrame> dst_frame) override;
  bool IsSupported() override;

 private:
  // Record for input/output buffers.
  struct BufferRecord {
    BufferRecord();
    ~BufferRecord();
    void* address[VIDEO_MAX_PLANES];  // mmap() address.
    size_t length[VIDEO_MAX_PLANES];  // mmap() length.

    // Set true during QBUF and DQBUF. |address| will be accessed by hardware.
    bool at_device;
  };

  void EnqueueInput();
  void EnqueueOutput();
  void Dequeue();
  bool EnqueueInputRecord();
  bool EnqueueOutputRecord();
  bool CreateInputBuffers();
  bool CreateOutputBuffers();
  void DestroyInputBuffers();
  void DestroyOutputBuffers();

  // Convert |output_buffer| to |dst_frame|. The function supports the following
  // formats:
  //   - All formats that libyuv::ConvertToI420 can handle.
  //   - V4L2_PIX_FMT_YUV_420M, V4L2_PIX_FMT_YUV_422M to I420, YV12, and NV12.
  bool ConvertOutputImage(const BufferRecord& output_buffer,
                          scoped_refptr<VideoFrame> dst_frame);

  // Return the number of input/output buffers enqueued to the device.
  size_t InputBufferQueuedCount();
  size_t OutputBufferQueuedCount();

  // Return true if input buffer size is not enough.
  bool ShouldRecreateInputBuffers();
  // Destroy and create input buffers. Return false on error.
  bool RecreateInputBuffers();
  // Destroy and create output buffers. Return false on error.
  bool RecreateOutputBuffers();

  void VideoFrameReady(int32_t task_id);
  void NotifyError(int32_t task_id, Error error);
  void PostNotifyError(int32_t task_id, Error error);

  // Run on |decoder_thread_| to enqueue the coming frame.
  void DecodeTask(std::unique_ptr<JobRecord> job_record);

  // Run on |decoder_thread_| to dequeue last frame and enqueue next frame.
  // This task is triggered by DevicePollTask. |event_pending| means that device
  // has resolution change event or pixelformat change event.
  void ServiceDeviceTask(bool event_pending);

  // Dequeue source change event. Return false on error.
  bool DequeueSourceChangeEvent();

  // Start/Stop |device_poll_thread_|.
  void StartDevicePoll();
  bool StopDevicePoll();

  // Run on |device_poll_thread_| to wait for device events.
  void DevicePollTask();

  // Run on |decoder_thread_| to destroy input and output buffers.
  void DestroyTask();

  // The number of input buffers and output buffers.
  const size_t kBufferCount = 2;

  // Coded size of output buffer.
  gfx::Size output_buffer_coded_size_;

  // Pixel format of output buffer.
  uint32_t output_buffer_pixelformat_;

  // Number of physical planes the output buffers have.
  size_t output_buffer_num_planes_;

  // Strides of the output buffers.
  size_t output_strides_[VIDEO_MAX_PLANES];

  // ChildThread's task runner.
  scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The client of this class.
  chromeos_camera::MjpegDecodeAccelerator::Client* client_;

  // The V4L2Device this class is operating upon.
  scoped_refptr<V4L2Device> device_;

  // Thread to communicate with the device.
  base::Thread decoder_thread_;
  // Decode task runner.
  scoped_refptr<base::SingleThreadTaskRunner> decoder_task_runner_;
  // Thread used to poll the V4L2 for events only.
  base::Thread device_poll_thread_;
  // Device poll task runner.
  scoped_refptr<base::SingleThreadTaskRunner> device_poll_task_runner_;

  // All the below members except |weak_factory_| are accessed from
  // |decoder_thread_| only (if it's running).
  base::queue<std::unique_ptr<JobRecord>> input_jobs_;
  base::queue<std::unique_ptr<JobRecord>> running_jobs_;

  // Input queue state.
  bool input_streamon_;
  // Mapping of int index to an input buffer record.
  std::vector<BufferRecord> input_buffer_map_;
  // Indices of input buffers ready to use; LIFO since we don't care about
  // ordering.
  std::vector<int> free_input_buffers_;

  // Output queue state.
  bool output_streamon_;
  // Mapping of int index to an output buffer record.
  std::vector<BufferRecord> output_buffer_map_;
  // Indices of output buffers ready to use; LIFO since we don't care about
  // ordering.
  std::vector<int> free_output_buffers_;

  // Point to |this| for use in posting tasks from the decoder thread back to
  // the ChildThread.
  base::WeakPtr<V4L2MjpegDecodeAccelerator> weak_ptr_;
  // Weak factory for producing weak pointers on the child thread.
  base::WeakPtrFactory<V4L2MjpegDecodeAccelerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2MjpegDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_MJPEG_DECODE_ACCELERATOR_H_
