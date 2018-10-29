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

#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/gpu/image_processor.h"
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
  void Encode(const scoped_refptr<VideoFrame>& frame,
              bool force_keyframe) override;
  void UseOutputBitstreamBuffer(const BitstreamBuffer& buffer) override;
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
    bool at_device;
    scoped_refptr<VideoFrame> frame;
  };

  // Record for output buffers.
  struct OutputRecord {
    OutputRecord();
    ~OutputRecord();
    bool at_device;
    std::unique_ptr<BitstreamBufferRef> buffer_ref;
    void* address;
    size_t length;
  };

  // Store all the information of input frame passed to Encode().
  struct InputFrameInfo {
    InputFrameInfo();
    InputFrameInfo(scoped_refptr<VideoFrame> frame, bool force_keyframe);
    InputFrameInfo(const InputFrameInfo&);
    ~InputFrameInfo();
    scoped_refptr<VideoFrame> frame;
    bool force_keyframe;
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
                      int output_buffer_index,
                      scoped_refptr<VideoFrame> frame);

  // Error callback for handling image processor errors.
  void ImageProcessorError();

  //
  // Encoding tasks, to be run on encode_thread_.
  //

  void EncodeTask(const scoped_refptr<VideoFrame>& frame, bool force_keyframe);

  // Add a BitstreamBuffer to the queue of buffers ready to be used for encoder
  // output.
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);

  // Device destruction task.
  void DestroyTask();

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

  // Change encoding parameters.
  void RequestEncodingParametersChangeTask(uint32_t bitrate,
                                           uint32_t framerate);

  // Set up formats and initialize the device for them.
  bool SetFormats(VideoPixelFormat input_format,
                  VideoCodecProfile output_profile);

  // Try to set up the device to the input format we were Initialized() with,
  // or if the device doesn't support it, use one it can support, so that we
  // can later instantiate an ImageProcessor to convert to it.
  bool NegotiateInputFormat(VideoPixelFormat input_format);

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

  // Recycle output buffer of image processor with |output_buffer_index|.
  void ReuseImageProcessorOutputBuffer(int output_buffer_index);

  // Copy encoded stream data from an output V4L2 buffer at |bitstream_data|
  // of size |bitstream_size| into a BitstreamBuffer referenced by |buffer_ref|,
  // injecting stream headers if required. Return the size in bytes of the
  // resulting stream in the destination buffer.
  size_t CopyIntoOutputBuffer(const uint8_t* bitstream_data,
                              size_t bitstream_size,
                              std::unique_ptr<BitstreamBufferRef> buffer_ref);

  // Our original calling task runner for the child thread.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  gfx::Size visible_size_;
  // Input allocated size required by the device.
  gfx::Size input_allocated_size_;
  size_t output_buffer_byte_size_;

  // Formats for input frames and the output stream.
  VideoPixelFormat device_input_format_;
  size_t input_planes_count_;
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

  // Input queue state.
  bool input_streamon_;
  // Input buffers enqueued to device.
  int input_buffer_queued_count_;
  // Input buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> free_input_buffers_;
  // Mapping of int index to input buffer record.
  std::vector<InputRecord> input_buffer_map_;
  v4l2_memory input_memory_type_;

  // Output queue state.
  bool output_streamon_;
  // Output buffers enqueued to device.
  int output_buffer_queued_count_;
  // Output buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> free_output_buffers_;
  // Mapping of int index to output buffer record.
  std::vector<OutputRecord> output_buffer_map_;

  // Bitstream buffers ready to be used to return encoded output, as a LIFO
  // since we don't care about ordering.
  std::vector<std::unique_ptr<BitstreamBufferRef>> encoder_output_queue_;

  // The completion callback of the Flush() function.
  FlushCallback flush_callback_;

  // Indicates whether the V4L2 device supports flush.
  // This is set in Initialize().
  bool is_flush_supported_;

  // Image processor, if one is in use.
  std::unique_ptr<ImageProcessor> image_processor_;
  // Indexes of free image processor output buffers. Only accessed on child
  // thread.
  std::vector<int> free_image_processor_output_buffers_;
  // Video frames ready to be processed. Only accessed on child thread.
  base::queue<InputFrameInfo> image_processor_input_queue_;

  // This thread services tasks posted from the VEA API entry points by the
  // child thread and device service callbacks posted from the device thread.
  base::Thread encoder_thread_;

  // The device polling thread handles notifications of V4L2 device changes.
  // TODO(sheu): replace this thread with an TYPE_IO encoder_thread_.
  base::Thread device_poll_thread_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  base::WeakPtr<Client> client_;
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the
  // image_processor_ back to the child thread.
  // Tasks posted onto encoder and poll threads can use base::Unretained(this),
  // as both threads will not outlive this object.
  base::WeakPtr<V4L2VideoEncodeAccelerator> weak_this_;
  base::WeakPtrFactory<V4L2VideoEncodeAccelerator> weak_this_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2VideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_ENCODE_ACCELERATOR_H_
