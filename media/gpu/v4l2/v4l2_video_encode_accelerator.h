// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_ENCODE_ACCELERATOR_H_

#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class BitstreamBuffer;

}  // namespace media

namespace media {

// This class handles video encode acceleration by interfacing with a V4L2
// device exposed by the codec hardware driver. The threading model of this
// class is the same as in the V4L2VideoDecodeAccelerator (from which class this
// was designed).
// This class may try to instantiate and use a ImageProcessor for input
// format conversion, if the input format requested via Initialize() is not
// accepted by the hardware codec.
class MEDIA_GPU_EXPORT V4L2VideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  explicit V4L2VideoEncodeAccelerator(const scoped_refptr<V4L2Device>& device);
  ~V4L2VideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config, Client* client) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;
  void Flush(FlushCallback flush_callback) override;
  bool IsFlushSupported() override;

 private:
  // Auto-destroy reference for BitstreamBuffer, for tracking buffers passed to
  // this instance.
  struct BitstreamBufferRef;

  // Record for codec input buffers.
  struct InputRecord {
    InputRecord();
    InputRecord(const InputRecord&);
    ~InputRecord();
    scoped_refptr<VideoFrame> frame;

    // This is valid only if image processor is used. The buffer associated with
    // this index can be reused in Dequeue().
    base::Optional<size_t> ip_output_buffer_index;
  };

  // Store all the information of input frame passed to Encode().
  struct InputFrameInfo {
    InputFrameInfo();
    InputFrameInfo(scoped_refptr<VideoFrame> frame, bool force_keyframe);
    InputFrameInfo(scoped_refptr<VideoFrame> frame,
                   bool force_keyframe,
                   size_t index);
    InputFrameInfo(const InputFrameInfo&);
    ~InputFrameInfo();
    scoped_refptr<VideoFrame> frame;
    bool force_keyframe;

    // This is valid only if image processor is used. This info needs to be
    // propagated to InputRecord.
    base::Optional<size_t> ip_output_buffer_index;
  };

  enum {
    // These are rather subjectively tuned.
    kInputBufferCount = 2,
    kOutputBufferCount = 2,
    kImageProcBufferCount = 2,
  };

  // Internal state of the encoder.
  enum State {
    kUninitialized,  // Initialize() not yet called.
    kInitialized,    // Initialize() returned true; ready to start encoding.
    kEncoding,       // Encoding frames.
    kFlushing,       // Flushing frames.
    kError,          // Error in encoder state.
  };

  //
  // Callbacks for the image processor, if one is used.
  //

  // Callback run by the image processor when a |frame| is ready for us to
  // encode.
  void FrameProcessed(bool force_keyframe,
                      base::TimeDelta timestamp,
                      size_t output_buffer_index,
                      scoped_refptr<VideoFrame> frame);

  // Error callback for handling image processor errors.
  void ImageProcessorError();

  //
  // Encoding tasks, to be run on encode_thread_.
  //

  void EncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Add a BitstreamBuffer to the queue of buffers ready to be used for encoder
  // output.
  void UseOutputBitstreamBufferTask(BitstreamBuffer buffer);

  // Device destruction task.
  void DestroyTask();

  // Try to output bitstream buffers.
  void PumpBitstreamBuffers();

  // Flush all the encoded frames. After all frames is flushed successfully or
  // any error occurs, |flush_callback| will be called to notify client.
  void FlushTask(FlushCallback flush_callback);

  // Service I/O on the V4L2 devices.  This task should only be scheduled from
  // DevicePollTask().
  void ServiceDeviceTask();

  // Handle the device queues.
  void Enqueue();
  void Dequeue();
  // Enqueue a buffer on the corresponding queue.  Returns false on fatal error.
  bool EnqueueInputRecord();
  bool EnqueueOutputRecord();

  // Attempt to start/stop device_poll_thread_.
  bool StartDevicePoll();
  bool StopDevicePoll();

  //
  // Device tasks, to be run on device_poll_thread_.
  //

  // The device task.
  void DevicePollTask(bool poll_device);

  //
  // Safe from any thread.
  //

  // Error notification (using PostTask() to child thread, if necessary).
  void NotifyError(Error error);

  // Set the encoder_state_ to kError and notify the client (if necessary).
  void SetErrorState(Error error);

  //
  // Other utility functions.  Called on encoder_thread_, unless
  // encoder_thread_ is not yet started, in which case the child thread can call
  // these (e.g. in Initialize() or Destroy()).
  //

  // Create image processor that will process input_layout to output_layout. The
  // visible size of processed video frames are |visible_size|.
  bool CreateImageProcessor(const VideoFrameLayout& input_layout,
                            const VideoFrameLayout& output_layout,
                            const gfx::Size& visible_size);
  // Process one video frame in |image_processor_input_queue_| by
  // |image_processor_|.
  void InputImageProcessorTask();

  // Change encoding parameters.
  void RequestEncodingParametersChangeTask(uint32_t bitrate,
                                           uint32_t framerate);

  // Do several initializations (e.g. set up format) on |encoder_thread_|.
  void InitializeTask(const Config& config,
                      bool* result,
                      base::WaitableEvent* done);

  // Set up formats and initialize the device for them.
  bool SetFormats(VideoPixelFormat input_format,
                  VideoCodecProfile output_profile);

  // Reconfigure format of input buffers and image processor if frame size
  // given by client is different from one set in input buffers.
  bool ReconfigureFormatIfNeeded(VideoPixelFormat format,
                                 const gfx::Size& new_frame_size);

  // Try to set up the device to the input format we were Initialized() with,
  // or if the device doesn't support it, use one it can support, so that we
  // can later instantiate an ImageProcessor to convert to it.
  bool NegotiateInputFormat(VideoPixelFormat input_format,
                            const gfx::Size& frame_size);

  // Set up the device to the output format requested in Initialize().
  bool SetOutputFormat(VideoCodecProfile output_profile);

  // Initialize device controls with |config| or default values.
  bool InitControls(const Config& config);

  // Create the buffers we need.
  bool CreateInputBuffers();
  bool CreateOutputBuffers();

  // Destroy these buffers.
  void DestroyInputBuffers();
  void DestroyOutputBuffers();

  // Set controls in |ctrls| and return true if successful.
  bool SetExtCtrls(std::vector<struct v4l2_ext_control> ctrls);

  // Return true if a V4L2 control of |ctrl_id| is supported by the device,
  // false otherwise.
  bool IsCtrlExposed(uint32_t ctrl_id);

  // Allocates |count| video frames with |visible_size| for image processor's
  // output buffers. Returns false if there's something wrong.
  bool AllocateImageProcessorOutputBuffers(size_t count,
                                           const gfx::Size& visible_size);

  // Recycle output buffer of image processor with |output_buffer_index|.
  void ReuseImageProcessorOutputBuffer(size_t output_buffer_index);

  // Copy encoded stream data from an output V4L2 buffer at |bitstream_data|
  // of size |bitstream_size| into a BitstreamBuffer referenced by |buffer_ref|,
  // injecting stream headers if required. Return the size in bytes of the
  // resulting stream in the destination buffer.
  size_t CopyIntoOutputBuffer(const uint8_t* bitstream_data,
                              size_t bitstream_size,
                              std::unique_ptr<BitstreamBufferRef> buffer_ref);

  // Initializes input_memory_type_.
  bool InitInputMemoryType(const Config& config);

  // Our original calling task runner for the child thread.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  gfx::Size visible_size_;
  // Layout of device accepted input VideoFrame.
  base::Optional<VideoFrameLayout> device_input_layout_;

  // Stands for whether an input buffer is native graphic buffer.
  bool native_input_mode_;

  // Input allocated size calculated by
  // V4L2Device::AllocatedSizeFromV4L2Format().
  // TODO(crbug.com/914700): Remove this once Client::RequireBitstreamBuffers
  // uses input's VideoFrameLayout to allocate input buffer.
  gfx::Size input_allocated_size_;

  size_t output_buffer_byte_size_;
  uint32_t output_format_fourcc_;

  //
  // Encoder state, owned and operated by encoder_thread_.
  // Before encoder_thread_ has started, the encoder state is managed by
  // the child (main) thread.  After encoder_thread_ has started, the encoder
  // thread should be the only one managing these.
  //

  // Encoder state.
  State encoder_state_;

  // For H264, for resilience, we prepend each IDR with SPS and PPS. Some
  // devices support this via the V4L2_CID_MPEG_VIDEO_H264_SPS_PPS_BEFORE_IDR
  // control. For devices that don't, we cache the latest SPS and PPS and inject
  // them into the stream before every IDR.
  bool inject_sps_and_pps_ = false;
  // Cached SPS (without H.264 start code).
  std::vector<uint8_t> cached_sps_;
  // Cached PPS (without H.264 start code).
  std::vector<uint8_t> cached_pps_;
  // Size in bytes required to inject cached SPS and PPS, including H.264
  // start codes.
  size_t cached_h264_header_size_ = 0;

  // Video frames ready to be encoded.
  base::queue<InputFrameInfo> encoder_input_queue_;

  // Encoder device.
  scoped_refptr<V4L2Device> device_;

  // Mapping of int index to input buffer record.
  std::vector<InputRecord> input_buffer_map_;
  v4l2_memory input_memory_type_;

  scoped_refptr<V4L2Queue> input_queue_;
  scoped_refptr<V4L2Queue> output_queue_;

  // Bitstream buffers ready to be used to return encoded output, as a LIFO
  // since we don't care about ordering.
  std::vector<std::unique_ptr<BitstreamBufferRef>> bitstream_buffer_pool_;

  // Queue of encoded bitstream V4L2 buffers. We enqueue the encoded buffers
  // from V4L2 devices, and copy the data to the bitstream buffers passed from
  // the client via UseOutputBitstreamBuffer().
  base::circular_deque<V4L2ReadableBufferRef> output_buffer_queue_;

  // The completion callback of the Flush() function.
  FlushCallback flush_callback_;

  // Indicates whether the V4L2 device supports flush.
  // This is set in Initialize().
  bool is_flush_supported_;

  // Image processor, if one is in use.
  std::unique_ptr<ImageProcessor> image_processor_;
  // Video frames for image processor output / VideoEncodeAccelerator input.
  // Only accessed on child thread.
  std::vector<scoped_refptr<VideoFrame>> image_processor_output_buffers_;
  // Indexes of free image processor output buffers. Only accessed on child
  // thread.
  std::vector<size_t> free_image_processor_output_buffer_indices_;
  // Video frames ready to be processed. Only accessed on child thread.
  base::queue<InputFrameInfo> image_processor_input_queue_;

  // This thread services tasks posted from the VideoEncodeAccelerator API entry
  // points by the child thread and device service callbacks posted from the
  // device thread.
  base::Thread encoder_thread_;

  // The device polling thread handles notifications of V4L2 device changes.
  // TODO(sheu): replace this thread with an TYPE_IO encoder_thread_.
  base::Thread device_poll_thread_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  base::WeakPtr<Client> client_;
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

  // WeakPtr<> pointing to |this| for use in posting tasks to
  // |encoder_thread_.task_runner()|. It guarantees no task will be executed
  // after DestroyTask().
  base::WeakPtr<V4L2VideoEncodeAccelerator> weak_this_;
  base::WeakPtrFactory<V4L2VideoEncodeAccelerator> weak_this_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2VideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_ENCODE_ACCELERATOR_H_
