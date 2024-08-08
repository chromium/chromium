// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_ENCODE_ACCELERATOR_H_

#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {
class BitstreamBuffer;

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
  explicit V4L2VideoEncodeAccelerator(scoped_refptr<V4L2Device> device);

  V4L2VideoEncodeAccelerator(const V4L2VideoEncodeAccelerator&) = delete;
  V4L2VideoEncodeAccelerator& operator=(const V4L2VideoEncodeAccelerator&) =
      delete;

  ~V4L2VideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config,
                  Client* client,
                  std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void RequestEncodingParametersChange(
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
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
    std::optional<size_t> ip_output_buffer_index;
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
    std::optional<size_t> ip_output_buffer_index;
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
    kInitialized,    // Initialize() returned true. The encoding is ready after
                     // InitializeTask() completes successfully.
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
  bool EnqueueInputRecord(V4L2WritableBufferRef input_buf);
  bool EnqueueOutputRecord(V4L2WritableBufferRef output_buf);

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

  // Set the encoder_state_ to kError and notify the client (if necessary).
  void SetErrorState(EncoderStatus status);

  //
  // Other utility functions.  Called on the |encoder_task_runner_|.
  //

  // Create image processor that will process |input_layout| +
  // |input_visible_rect| to |output_layout|+|output_visible_rect|.
  bool CreateImageProcessor(const VideoFrameLayout& input_layout,
                            const VideoPixelFormat output_format,
                            const gfx::Size& output_size,
                            const gfx::Rect& input_visible_rect,
                            const gfx::Rect& output_visible_rect);
  // Process one video frame in |image_processor_input_queue_| by
  // |image_processor_|.
  void InputImageProcessorTask();

  void MaybeFlushImageProcessor();

  // Change encoding parameters.
  void RequestEncodingParametersChangeTask(
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate,
      const std::optional<gfx::Size>& size);

  // Do several initializations (e.g. set up format) on |encoder_task_runner_|.
  void InitializeTask(const Config& config);

  // Set up formats and initialize the device for them.
  bool SetFormats(VideoPixelFormat input_format,
                  VideoCodecProfile output_profile);

  // Reconfigure format of input buffers and image processor if the buffer
  // represented by |frame| is different from one set in input buffers.
  bool ReconfigureFormatIfNeeded(const VideoFrame& frame);

  // Try to set up the device to the input format we were Initialized() with,
  // or if the device doesn't support it, use one it can support, so that we
  // can later instantiate an ImageProcessor to convert to it. Return
  // std::nullopt if no format is supported, otherwise return v4l2_format
  // adjusted by the driver.
  std::optional<struct v4l2_format> NegotiateInputFormat(
      VideoPixelFormat input_format,
      const gfx::Size& frame_size);

  // Apply the current crop parameters to the V4L2 device.
  bool ApplyCrop();

  // Set up the device to the output format requested in Initialize().
  bool SetOutputFormat(VideoCodecProfile output_profile);

  // Initialize device controls with |config| or default values.
  bool InitControls(const Config& config);

  // Initialize device controls with |config| or default values.
  bool InitControlsH264(const Config& config);

  // Initialize device controls with |config| or default values.
  void InitControlsVP8(const Config& config);

  // Create the buffers we need.
  bool CreateInputBuffers();
  bool CreateOutputBuffers();

  // Destroy these buffers.
  void DestroyInputBuffers();
  void DestroyOutputBuffers();

  // Allocates |count| video frames with |visible_size| for image processor's
  // output buffers. Returns false if there's something wrong.
  bool AllocateImageProcessorOutputBuffers(size_t count);

  // Recycle output buffer of image processor with |output_buffer_index|.
  void ReuseImageProcessorOutputBuffer(size_t output_buffer_index);

  // Chrome specific metadata about the encoded frame.
  BitstreamBufferMetadata GetMetadata(const uint8_t* data,
                                      size_t data_size_bytes,
                                      bool key_frame,
                                      base::TimeDelta timestamp);

  // Copy encoded stream data from an output V4L2 buffer at |bitstream_data|
  // of size |bitstream_size| into a BitstreamBuffer referenced by |buffer_ref|,
  // injecting stream headers if required. Return the size in bytes of the
  // resulting stream in the destination buffer.
  size_t CopyIntoOutputBuffer(const uint8_t* bitstream_data,
                              size_t bitstream_size,
                              std::unique_ptr<BitstreamBufferRef> buffer_ref);

  // Initializes input_memory_type_.
  bool InitInputMemoryType(const Config& config);

  // Having too many encoder instances at once may cause us to run out of FDs
  // and subsequently crash (crbug.com/1289465). To avoid that, we limit the
  // maximum number of encoder instances that can exist at once.
  // |num_instances_| tracks that number.
  static constexpr int kMaxNumOfInstances = 10;
  static base::AtomicRefCount num_instances_;
  const bool can_use_encoder_;

  std::string driver_name_;

  // Our original calling task runner for the child sequence  and its checker.
  const scoped_refptr<base::SequencedTaskRunner> child_task_runner_;
  SEQUENCE_CHECKER(child_sequence_checker_);

  // A coded_size() of VideoFrame on VEA::Encode(). This is updated on the first
  // time Encode() if the coded size is different from the expected one by VEA.
  // For example, it happens in WebRTC simulcast case.
  gfx::Size input_frame_size_;
  // A natural_size() of VideoFrame on VEA::Encode(). This is updated on the
  // first time Encode() always. The natural_size() of VideoFrames fed by
  // VEA::Encode() must be the same as |input_natural_size_|.
  gfx::Size input_natural_size_;

  // Visible rectangle of VideoFrame to be fed to an encoder driver, in other
  // words, a visible rectangle that output encoded bitstream buffers represent.
  gfx::Rect encoder_input_visible_rect_;

  // Layout of device accepted input VideoFrame.
  std::optional<VideoFrameLayout> device_input_layout_;

  // Stands for whether an input buffer is native graphic buffer.
  bool native_input_mode_;

  size_t output_buffer_byte_size_;
  uint32_t output_format_fourcc_;

  VideoBitrateAllocation current_bitrate_allocation_;
  size_t current_framerate_;

  // Encoder state, owned and operated by |encoder_task_runner_|.
  State encoder_state_;

  // For H264, for resilience, we prepend each IDR with SPS and PPS. Some
  // devices support this via the V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR
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
  // Indexes of free image processor output buffers. Only accessed on
  // |child_task_runner_|.
  std::vector<size_t> free_image_processor_output_buffer_indices_;
  // Video frames ready to be processed. Only accessed on |child_task_runner_|.
  base::queue<InputFrameInfo> image_processor_input_queue_;
  // The number of frames that are being processed by |image_processor_|.
  size_t num_frames_in_image_processor_ = 0;

  // Indicates whether V4L2VideoEncodeAccelerator runs in L1T2 or not.
  bool h264_l1t2_enabled_ = false;

  const scoped_refptr<base::SequencedTaskRunner> encoder_task_runner_;
  SEQUENCE_CHECKER(encoder_sequence_checker_);

  // The device polling thread handles notifications of V4L2 device changes.
  // TODO(sheu): replace this thread with an TYPE_IO encoder_thread_.
  base::Thread device_poll_thread_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // |child_task_runner_|.
  base::WeakPtr<Client> client_;
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

  // WeakPtr<> pointing to |this| for use in posting tasks to
  // |encoder_task_runner_|.
  base::WeakPtr<V4L2VideoEncodeAccelerator> weak_this_;
  base::WeakPtrFactory<V4L2VideoEncodeAccelerator> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_ENCODE_ACCELERATOR_H_
