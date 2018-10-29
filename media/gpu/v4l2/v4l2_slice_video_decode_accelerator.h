// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_

#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/vp8_decoder.h"
#include "media/gpu/vp9_decoder.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gl/gl_fence_egl.h"

namespace media {

class V4L2DecodeSurface;

// An implementation of VideoDecodeAccelerator that utilizes the V4L2 slice
// level codec API for decoding. The slice level API provides only a low-level
// decoding functionality and requires userspace to provide support for parsing
// the input stream and managing decoder state across frames.
class MEDIA_GPU_EXPORT V4L2SliceVideoDecodeAccelerator
    : public VideoDecodeAccelerator,
      public DecodeSurfaceHandler<V4L2DecodeSurface> {
 public:
  V4L2SliceVideoDecodeAccelerator(
      const scoped_refptr<V4L2Device>& device,
      EGLDisplay egl_display,
      const BindGLImageCallback& bind_image_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb);
  ~V4L2SliceVideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation.
  bool Initialize(const Config& config, Client* client) override;
  void Decode(const BitstreamBuffer& bitstream_buffer) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              int32_t bitstream_id) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ImportBufferForPicture(
      int32_t picture_buffer_id,
      VideoPixelFormat pixel_format,
      const gfx::GpuMemoryBufferHandle& gpu_memory_buffer_handle) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateThread(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner)
      override;

  static VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles();

 private:
  class V4L2H264Accelerator;
  class V4L2VP8Accelerator;
  class V4L2VP9Accelerator;

  // Record for input buffers.
  struct InputRecord {
    InputRecord();
    int32_t input_id;
    void* address;
    size_t length;
    size_t bytes_used;
    bool at_device;
  };

  // Record for output buffers.
  struct OutputRecord {
    OutputRecord();
    OutputRecord(OutputRecord&&);
    ~OutputRecord();
    bool at_device;
    bool at_client;
    size_t num_times_sent_to_client;
    int32_t picture_id;
    GLuint client_texture_id;
    GLuint texture_id;
    std::unique_ptr<gl::GLFenceEGL> egl_fence;
    std::vector<base::ScopedFD> dmabuf_fds;
    bool cleared;
  };

  // Decoder state enum.
  enum State {
    // We are in this state until Initialize() returns successfully.
    // We can't post errors to the client in this state yet.
    kUninitialized,
    // Initialize() returned successfully.
    kInitialized,
    // This state allows making progress decoding more input stream.
    kDecoding,
    // Transitional state when we are not decoding any more stream, but are
    // performing flush, reset, resolution change or are destroying ourselves.
    kIdle,
    // Requested new PictureBuffers via ProvidePictureBuffers(), awaiting
    // AssignPictureBuffers().
    kAwaitingPictureBuffers,
    // Error state, set when sending NotifyError to client.
    kError,
    // Destroying state, when shutting down the decoder.
    kDestroying,
  };

  // See http://crbug.com/255116.
  // Input bitstream buffer size for up to 1080p streams.
  const size_t kInputBufferMaxSizeFor1080p = 1024 * 1024;
  // Input bitstream buffer size for up to 4k streams.
  const size_t kInputBufferMaxSizeFor4k = 4 * kInputBufferMaxSizeFor1080p;
  const size_t kNumInputBuffers = 16;

  // Input format V4L2 fourccs this class supports.
  static const uint32_t supported_input_fourccs_[];

  //
  // Below methods are used by accelerator implementations.
  //
  // DecodeSurfaceHandler implementation.
  scoped_refptr<V4L2DecodeSurface> CreateSurface() override;
  // SurfaceReady() uses |decoder_display_queue_| to guarantee that decoding
  // of |dec_surface| happens in order.
  void SurfaceReady(const scoped_refptr<V4L2DecodeSurface>& dec_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& /* color_space */) override;

  // Append slice data in |data| of size |size| to pending hardware
  // input buffer with |index|. This buffer will be submitted for decode
  // on the next DecodeSurface(). Return true on success.
  bool SubmitSlice(int index, const uint8_t* data, size_t size);

  // Submit controls in |ext_ctrls| to hardware. Return true on success.
  bool SubmitExtControls(struct v4l2_ext_controls* ext_ctrls);

  // Gets current control values for controls in |ext_ctrls| from the driver.
  // Return true on success.
  bool GetExtControls(struct v4l2_ext_controls* ext_ctrls);

  // Return true if the driver exposes V4L2 control |ctrl_id|, false otherwise.
  bool IsCtrlExposed(uint32_t ctrl_id);

  //
  // Internal methods of this class.
  //
  // Recycle a V4L2 input buffer with |index| after dequeuing from device.
  void ReuseInputBuffer(int index);

  // Recycle V4L2 output buffer with |index|. Used as surface release callback.
  void ReuseOutputBuffer(int index);

  // Queue a |dec_surface| to device for decoding.
  void Enqueue(const scoped_refptr<V4L2DecodeSurface>& dec_surface);

  // Dequeue any V4L2 buffers available and process.
  void Dequeue();

  // V4L2 QBUF helpers.
  bool EnqueueInputRecord(int index, uint32_t config_store);
  bool EnqueueOutputRecord(int index);

  // Set input and output formats in hardware.
  bool SetupFormats();

  // Create input and output buffers.
  bool CreateInputBuffers();
  bool CreateOutputBuffers();

  // Destroy input buffers.
  void DestroyInputBuffers();

  // Destroy output buffers and release associated resources (textures,
  // EGLImages). If |dismiss| is true, also dismissing the associated
  // PictureBuffers.
  bool DestroyOutputs(bool dismiss);

  // Used by DestroyOutputs.
  bool DestroyOutputBuffers();

  // Dismiss all |picture_buffer_ids| via Client::DismissPictureBuffer()
  // and signal |done| after finishing.
  void DismissPictures(const std::vector<int32_t>& picture_buffer_ids,
                       base::WaitableEvent* done);

  // Task to finish initialization on decoder_thread_.
  void InitializeTask();

  void NotifyError(Error error);
  void DestroyTask();

  // Check whether a destroy is scheduled.
  bool IsDestroyPending();

  // Sets the state to kError and notifies client if needed.
  void SetErrorState(Error error);

  // Event handling. Events include flush, reset and resolution change and are
  // processed while in kIdle state.

  // Surface set change (resolution change) flow.
  // If we have no surfaces allocated, start it immediately, otherwise mark
  // ourselves as pending for surface set change.
  void InitiateSurfaceSetChange();
  // If a surface set change is pending and we are ready, stop the device,
  // destroy outputs, releasing resources and dismissing pictures as required,
  // followed by starting the flow to allocate a new set for the current
  // resolution/DPB size, as provided by decoder.
  bool FinishSurfaceSetChange();

  // Flush flow when requested by client.
  // When Flush() is called, it posts a FlushTask, which checks the input queue.
  // If nothing is pending for decode on decoder_input_queue_, we call
  // InitiateFlush() directly. Otherwise, we push a dummy BitstreamBufferRef
  // onto the decoder_input_queue_ to schedule a flush. When we reach it later
  // on, we call InitiateFlush() to perform it at the correct time.
  void FlushTask();
  // Tell the decoder to flush all frames, reset it and mark us as scheduled
  // for flush, so that we can finish it once all pending decodes are finished.
  void InitiateFlush();
  // To be called if decoder_flushing_ is true. If not all pending frames are
  // decoded, return false, requesting the caller to try again later.
  // Otherwise perform flush by sending all pending pictures to the client,
  // notify it that flush is finished and return true, informing the caller
  // that further progress can be made.
  bool FinishFlush();

  // Reset flow when requested by client.
  // Drop all inputs, reset the decoder and mark us as pending for reset.
  void ResetTask();
  // To be called if decoder_resetting_ is true. If not all pending frames are
  // decoded, return false, requesting the caller to try again later.
  // Otherwise perform reset by dropping all pending outputs (client is not
  // interested anymore), notifying it that reset is finished, and return true,
  // informing the caller that further progress can be made.
  bool FinishReset();

  // Called when a new event is pended. Transitions us into kIdle state (if not
  // already in it), if possible. Also starts processing events.
  void NewEventPending();

  // Called after all events are processed successfully (i.e. all Finish*()
  // methods return true) to return to decoding state.
  bool FinishEventProcessing();

  // Process pending events, if any.
  void ProcessPendingEventsIfNeeded();

  // Allocate V4L2 buffers and assign them to |buffers| provided by the client
  // via AssignPictureBuffers() on decoder thread.
  void AssignPictureBuffersTask(const std::vector<PictureBuffer>& buffers);

  // Use buffer backed by dmabuf file descriptors in |passed_dmabuf_fds| for the
  // OutputRecord associated with |picture_buffer_id|, taking ownership of the
  // file descriptors.
  void ImportBufferForPictureTask(
      int32_t picture_buffer_id,
      // TODO(posciak): (https://crbug.com/561749) we should normally be able to
      // pass the vector by itself via std::move, but it's not possible to do
      // this if this method is used as a callback.
      std::unique_ptr<std::vector<base::ScopedFD>> passed_dmabuf_fds);

  // Create a GLImage for the buffer associated with V4L2 |buffer_index| and
  // for |picture_buffer_id|, backed by dmabuf file descriptors in
  // |passed_dmabuf_fds|, taking ownership of them.
  // The GLImage will be associated |client_texture_id| in gles2 decoder.
  void CreateGLImageFor(
      size_t buffer_index,
      int32_t picture_buffer_id,
      // TODO(posciak): (https://crbug.com/561749) we should normally be able to
      // pass the vector by itself via std::move, but it's not possible to do
      // this if this method is used as a callback.
      std::unique_ptr<std::vector<base::ScopedFD>> passed_dmabuf_fds,
      GLuint client_texture_id,
      GLuint texture_id,
      const gfx::Size& size,
      uint32_t fourcc);

  // Take the dmabuf |passed_dmabuf_fds|, for |picture_buffer_id|, and use it
  // for OutputRecord at |buffer_index|. The buffer is backed by
  // |passed_dmabuf_fds|, and the OutputRecord takes ownership of them.
  void AssignDmaBufs(
      size_t buffer_index,
      int32_t picture_buffer_id,
      // TODO(posciak): (https://crbug.com/561749) we should normally be able to
      // pass the vector by itself via std::move, but it's not possible to do
      // this if this method is used as a callback.
      std::unique_ptr<std::vector<base::ScopedFD>> passed_dmabuf_fds);

  // Performed on decoder_thread_ as a consequence of poll() on decoder_thread_
  // returning an event.
  void ServiceDeviceTask();

  // Schedule poll if we have any buffers queued and the poll thread
  // is not stopped (on surface set change).
  void SchedulePollIfNeeded();

  // Attempt to start/stop device_poll_thread_.
  bool StartDevicePoll();
  bool StopDevicePoll(bool keep_input_state);

  // Ran on device_poll_thread_ to wait for device events.
  void DevicePollTask(bool poll_device);

  // Buffer id for flush buffer, queued by FlushTask().
  const int kFlushBufferId = -2;

  // Handler for Decode() on decoder_thread_.
  void DecodeTask(scoped_refptr<DecoderBuffer> buffer, int32_t bitstream_id);

  // Schedule a new DecodeBufferTask if we are decoding.
  void ScheduleDecodeBufferTaskIfNeeded();

  // Main decoder loop. Keep decoding the current buffer in decoder_, asking
  // for more stream via TrySetNewBistreamBuffer() if decoder_ requests so,
  // and handle other returns from it appropriately.
  void DecodeBufferTask();

  // Check decoder_input_queue_ for any available buffers to decode and
  // set the decoder_current_bitstream_buffer_ to the next buffer if one is
  // available, taking it off the queue. Also set the current stream pointer
  // in decoder_, and return true.
  // Return false if no buffers are pending on decoder_input_queue_.
  bool TrySetNewBistreamBuffer();

  // Task to flag the specified picture buffer for reuse, executed on the
  // decoder_thread_. The picture buffer can only be reused after the specified
  // fence has been signaled.
  void ReusePictureBufferTask(int32_t picture_buffer_id,
                              std::unique_ptr<gl::GLFenceEGL> egl_fence);

  // Called to actually send |dec_surface| to the client - as a result of
  // decoding the stream in |bitstream_id| - after it is decoded preserving
  // the order in which it was scheduled via SurfaceReady().
  void OutputSurface(int32_t bitstream_id,
                     const scoped_refptr<V4L2DecodeSurface>& dec_surface);

  // Goes over the |decoder_display_queue_| and sends all buffers from the
  // front of the queue that are already decoded to the client, in order.
  void TryOutputSurfaces();

  // Send decoded pictures to PictureReady.
  void SendPictureReady();

  // Callback that indicates a picture has been cleared.
  void PictureCleared();

  size_t input_planes_count_;
  size_t output_planes_count_;

  // GPU Child thread task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // Task runner Decode() and PictureReady() run on.
  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the decoder or
  // device worker threads back to the child thread.
  base::WeakPtr<V4L2SliceVideoDecodeAccelerator> weak_this_;

  // To expose client callbacks from VideoDecodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  std::unique_ptr<base::WeakPtrFactory<VideoDecodeAccelerator::Client>>
      client_ptr_factory_;
  base::WeakPtr<VideoDecodeAccelerator::Client> client_;
  // Callbacks to |decode_client_| must be executed on |decode_task_runner_|.
  base::WeakPtr<Client> decode_client_;

  // V4L2 device in use.
  scoped_refptr<V4L2Device> device_;

  // Thread to communicate with the device on.
  base::Thread decoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> decoder_thread_task_runner_;

  // Thread used to poll the device for events.
  base::Thread device_poll_thread_;

  // Input queue state.
  bool input_streamon_;
  // Number of input buffers enqueued to the device.
  int input_buffer_queued_count_;
  // Input buffers ready to use; LIFO since we don't care about ordering.
  std::list<int> free_input_buffers_;
  // Mapping of int index to an input buffer record.
  std::vector<InputRecord> input_buffer_map_;

  // Output queue state.
  bool output_streamon_;
  // Number of output buffers enqueued to the device.
  int output_buffer_queued_count_;
  // Output buffers ready to use.
  std::list<int> free_output_buffers_;
  // Mapping of int index to an output buffer record.
  std::vector<OutputRecord> output_buffer_map_;

  VideoCodecProfile video_profile_;
  uint32_t input_format_fourcc_;
  uint32_t output_format_fourcc_;
  gfx::Size coded_size_;

  struct BitstreamBufferRef;
  // Input queue of stream buffers coming from the client.
  base::queue<std::unique_ptr<BitstreamBufferRef>> decoder_input_queue_;
  // BitstreamBuffer currently being processed.
  std::unique_ptr<BitstreamBufferRef> decoder_current_bitstream_buffer_;

  // Queue storing decode surfaces ready to be output as soon as they are
  // decoded, together with the bitstream_id from which they were decoded,
  // in order to be able to pass it back to the client.
  // The surfaces must be output in order they are queued.
  base::queue<std::pair<int32_t, scoped_refptr<V4L2DecodeSurface>>>
      decoder_display_queue_;

  // Decoder state.
  State state_;

  // Waitable event signaled when the decoder is destroying.
  base::WaitableEvent destroy_pending_;

  Config::OutputMode output_mode_;

  // If any of these are true, we are waiting for the device to finish decoding
  // all previously-queued frames, so we can finish the flush/reset/surface
  // change flows. These can stack.
  bool decoder_flushing_;
  bool decoder_resetting_;
  bool surface_set_change_pending_;

  // Codec-specific software decoder in use.
  std::unique_ptr<AcceleratedVideoDecoder> decoder_;

  // Surfaces queued to device to keep references to them while decoded.
  using V4L2DecodeSurfaceByOutputId =
      std::map<int, scoped_refptr<V4L2DecodeSurface>>;
  V4L2DecodeSurfaceByOutputId surfaces_at_device_;

  // Surfaces sent to client to keep references to them while displayed.
  using V4L2DecodeSurfaceByPictureBufferId =
      std::map<int32_t, scoped_refptr<V4L2DecodeSurface>>;
  V4L2DecodeSurfaceByPictureBufferId surfaces_at_display_;

  // Record for decoded pictures that can be sent to PictureReady.
  struct PictureRecord {
    PictureRecord(bool cleared, const Picture& picture);
    ~PictureRecord();
    bool cleared;     // Whether the texture is cleared and safe to render from.
    Picture picture;  // The decoded picture.
  };

  // Pictures that are ready but not sent to PictureReady yet.
  base::queue<PictureRecord> pending_picture_ready_;

  // The number of pictures that are sent to PictureReady and will be cleared.
  int picture_clearing_count_;

  // EGL state
  EGLDisplay egl_display_;

  // Callback to bind a GLImage.
  BindGLImageCallback bind_image_cb_;
  // Callback to set the correct gl context.
  MakeGLContextCurrentCallback make_context_current_cb_;

  // The WeakPtrFactory for |weak_this_|.
  base::WeakPtrFactory<V4L2SliceVideoDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2SliceVideoDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_
