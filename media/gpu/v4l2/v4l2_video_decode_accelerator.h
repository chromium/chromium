// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VideoDecodeAccelerator
// that utilizes hardware video decoders, which expose Video4Linux 2 API
// (http://linuxtv.org/downloads/v4l-dvb-apis/).

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_provider.h"
#include "media/base/limits.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence_egl.h"

namespace gl {

class GLFenceEGL;

}  // namespace gl

namespace media {

class H264Parser;
class V4L2StatefulWorkaround;

// This class handles video accelerators directly through a V4L2 device exported
// by the hardware blocks.
//
// The threading model of this class is driven by the fact that it needs to
// interface two fundamentally different event queues -- the one Chromium
// provides through MessageLoop, and the one driven by the V4L2 devices which
// is waited on with epoll().  There are three threads involved in this class:
//
// * The child thread, which is the main GPU process thread which calls the
//   VideoDecodeAccelerator entry points.  Calls from this thread
//   generally do not block (with the exception of Initialize() and Destroy()).
//   They post tasks to the decoder_thread_, which actually services the task
//   and calls back when complete through the
//   VideoDecodeAccelerator::Client interface.
// * The decoder_thread_, owned by this class.  It services API tasks, through
//   the *Task() routines, as well as V4L2 device events, through
//   ServiceDeviceTask().  Almost all state modification is done on this thread
//   (this doesn't include buffer (re)allocation sequence, see below).
// * The device_poll_thread_, owned by this class.  All it does is epoll() on
//   the V4L2 in DevicePollTask() and schedule a ServiceDeviceTask() on the
//   decoder_thread_ when something interesting happens.
//   TODO(sheu): replace this thread with an TYPE_IO decoder_thread_.
//
// Note that this class has (almost) no locks, apart from the pictures_assigned_
// WaitableEvent. Everything (apart from buffer (re)allocation) is serviced on
// the decoder_thread_, so there are no synchronization issues.
// ... well, there are, but it's a matter of getting messages posted in the
// right order, not fiddling with locks.
// Buffer creation is a two-step process that is serviced partially on the
// Child thread, because we need to wait for the client to provide textures
// for the buffers we allocate. We cannot keep the decoder thread running while
// the client allocates Pictures for us, because we need to REQBUFS first to get
// the required number of output buffers from the device and that cannot be done
// unless we free the previous set of buffers, leaving the decoding in a
// inoperable state for the duration of the wait for Pictures. So to prevent
// subtle races (esp. if we get Reset() in the meantime), we block the decoder
// thread while we wait for AssignPictureBuffers from the client.
//
// V4L2VideoDecodeAccelerator may use image processor to convert the output.
// There are three cases:
// Flush: V4L2VDA should wait until image processor returns all processed
//   frames.
// Reset: V4L2VDA doesn't need to wait for image processor. When image processor
//   returns an old frame, drop it.
// Resolution change: V4L2VDA destroy image processor when destroying output
//   buffrers. We cannot drop any frame during resolution change. So V4L2VDA
//   should destroy output buffers after image processor returns all the frames.
class MEDIA_GPU_EXPORT V4L2VideoDecodeAccelerator
    : public VideoDecodeAccelerator,
      public base::trace_event::MemoryDumpProvider {
 public:
  V4L2VideoDecodeAccelerator(
      EGLDisplay egl_display,
      const GetGLContextCallback& get_gl_context_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const scoped_refptr<V4L2Device>& device);
  ~V4L2VideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation.
  // Note: Initialize() and Destroy() are synchronous.
  bool Initialize(const Config& config, Client* client) override;
  void Decode(BitstreamBuffer bitstream_buffer) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              int32_t bitstream_id) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ImportBufferForPicture(
      int32_t picture_buffer_id,
      VideoPixelFormat pixel_format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handles) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateThread(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner)
      override;

  static VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // These are rather subjectively tuned.
  enum {
    kInputBufferCount = 8,
    // TODO(posciak): determine input buffer size based on level limits.
    // See http://crbug.com/255116.
    // Input bitstream buffer size for up to 1080p streams.
    kInputBufferMaxSizeFor1080p = 1024 * 1024,
    // Input bitstream buffer size for up to 4k streams.
    kInputBufferMaxSizeFor4k = 4 * kInputBufferMaxSizeFor1080p,
    // Number of output buffers to use for each VDA stage above what's required
    // by the decoder (e.g. DPB size, in H264).  We need
    // limits::kMaxVideoFrames to fill up the GpuVideoDecode pipeline,
    // and +1 for a frame in transit.
    kDpbOutputBufferExtraCount = limits::kMaxVideoFrames + 1,
    // Number of extra output buffers if image processor is used.
    kDpbOutputBufferExtraCountForImageProcessor = 1,
  };

  // Internal state of the decoder.
  enum State {
    kUninitialized,  // Initialize() not yet called.
    kInitialized,    // Initialize() returned true; ready to start decoding.
    kDecoding,       // DecodeBufferInitial() successful; decoding frames.
    kResetting,      // Presently resetting.
    // Performing resolution change and waiting for image processor to return
    // all frames.
    kChangingResolution,
    // Requested new PictureBuffers via ProvidePictureBuffers(), awaiting
    // AssignPictureBuffers().
    kAwaitingPictureBuffers,
    kError,       // Error in kDecoding state.
    kDestroying,  // Destroying state, when shutting down the decoder.
  };

  enum BufferId {
    kFlushBufferId = -2  // Buffer id for flush buffer, queued by FlushTask().
  };

  // Auto-destruction reference for BitstreamBuffer, for message-passing from
  // Decode() to DecodeTask().
  struct BitstreamBufferRef;

  // Record for decoded pictures that can be sent to PictureReady.
  struct PictureRecord {
    PictureRecord(bool cleared, const Picture& picture);
    ~PictureRecord();
    bool cleared;     // Whether the texture is cleared and safe to render from.
    Picture picture;  // The decoded picture.
  };

  // Record for output buffers.
  struct OutputRecord {
    OutputRecord();
    OutputRecord(OutputRecord&&);
    ~OutputRecord();
    EGLImageKHR egl_image;  // EGLImageKHR for the output buffer.
    int32_t picture_id;     // picture buffer id as returned to PictureReady().
    GLuint texture_id;
    bool cleared;           // Whether the texture is cleared and safe to render
                            // from. See TextureManager for details.
    // Output frame. Used only when OutputMode is IMPORT.
    scoped_refptr<VideoFrame> output_frame;
  };

  //
  // Decoding tasks, to be run on decode_thread_.
  //

  // Task to finish initialization on decoder_thread_.
  void InitializeTask(const Config& config,
                      bool* result,
                      base::WaitableEvent* done);
  bool CheckConfig(const Config& config);

  // Enqueue a buffer to decode.  This will enqueue a buffer to the
  // decoder_input_queue_, then queue a DecodeBufferTask() to actually decode
  // the buffer.
  void DecodeTask(scoped_refptr<DecoderBuffer> buffer, int32_t bitstream_id);

  // Decode from the buffers queued in decoder_input_queue_.  Calls
  // DecodeBufferInitial() or DecodeBufferContinue() as appropriate.
  void DecodeBufferTask();
  // Advance to the next fragment that begins a frame.
  bool AdvanceFrameFragment(const uint8_t* data, size_t size, size_t* endpos);
  // Schedule another DecodeBufferTask() if we're behind.
  void ScheduleDecodeBufferTaskIfNeeded();

  // Return true if we should continue to schedule DecodeBufferTask()s after
  // completion.  Store the amount of input actually consumed in |endpos|.
  bool DecodeBufferInitial(const void* data, size_t size, size_t* endpos);
  bool DecodeBufferContinue(const void* data, size_t size);

  // Accumulate data for the next frame to decode.  May return false in
  // non-error conditions; for example when pipeline is full and should be
  // retried later.
  bool AppendToInputFrame(const void* data, size_t size);
  // Flush data for one decoded frame.
  bool FlushInputFrame();

  // Allocate V4L2 buffers and assign them to |buffers| provided by the client
  // via AssignPictureBuffers() on decoder thread.
  void AssignPictureBuffersTask(const std::vector<PictureBuffer>& buffers);

  // Use buffer backed by dmabuf file descriptors in |dmabuf_fds| for the
  // OutputRecord associated with |picture_buffer_id|, taking ownership of the
  // file descriptors. |stride| is the number of bytes from one row of pixels
  // to the next row.
  void ImportBufferForPictureTask(int32_t picture_buffer_id,
                                  std::vector<base::ScopedFD> dmabuf_fds,
                                  int32_t stride);

  // Check |planes| and |dmabuf_fds| are valid in import mode, besides
  // ImportBufferForPicture.
  void ImportBufferForPictureForImportTask(int32_t picture_buffer_id,
                                           VideoPixelFormat pixel_format,
                                           gfx::NativePixmapHandle handle);

  // Create an EGLImage for the buffer associated with V4L2 |buffer_index| and
  // for |picture_buffer_id|, backed by dmabuf file descriptors in
  // |passed_dmabuf_fds|, taking ownership of them.
  // The buffer should be bound to |texture_id| and is of |size| and format
  // described by |fourcc|.
  void CreateEGLImageFor(size_t buffer_index,
                         int32_t picture_buffer_id,
                         std::vector<base::ScopedFD> dmabuf_fds,
                         GLuint texture_id,
                         const gfx::Size& size,
                         uint32_t fourcc);

  // Take the EGLImage |egl_image|, created for |picture_buffer_id|, and use it
  // for OutputRecord at |buffer_index|.
  void AssignEGLImage(size_t buffer_index,
                      int32_t picture_buffer_id,
                      EGLImageKHR egl_image);

  // Service I/O on the V4L2 devices.  This task should only be scheduled from
  // DevicePollTask().  If |event_pending| is true, one or more events
  // on file descriptor are pending.
  void ServiceDeviceTask(bool event_pending);

  // Release buffers awaiting for their fence to be signaled.
  void CheckGLFences();
  // Handle the various device queues.
  void Enqueue();
  void Dequeue();
  // Dequeue one input buffer. Return true if success.
  bool DequeueInputBuffer();
  // Dequeue one output buffer. Return true if success.
  bool DequeueOutputBuffer();

  // Return true if there is a resolution change event pending.
  bool DequeueResolutionChangeEvent();

  // Enqueue a buffer on the corresponding queue.
  bool EnqueueInputRecord();
  bool EnqueueOutputRecord();

  // Task to flag the specified picture buffer for reuse, executed on the
  // decoder_thread_. The picture buffer can only be reused after the specified
  // fence has been signaled.
  void ReusePictureBufferTask(int32_t picture_buffer_id,
                              std::unique_ptr<gl::GLFenceEGL> egl_fence);

  // Flush() task.  Child thread should not submit any more buffers until it
  // receives the NotifyFlushDone callback.  This task will schedule an empty
  // BitstreamBufferRef (with input_id == kFlushBufferId) to perform the flush.
  void FlushTask();
  // Notify the client of a flush completion, if required.  This should be
  // called any time a relevant queue could potentially be emptied: see
  // function definition.
  void NotifyFlushDoneIfNeeded();
  // Notify the client of a flush completion.
  void NofityFlushDone();
  // Returns true if VIDIOC_DECODER_CMD is supported.
  bool IsDecoderCmdSupported();
  // Send V4L2_DEC_CMD_START to the driver. Return true if success.
  bool SendDecoderCmdStop();

  // Reset() task.  Drop all input buffers. If V4L2VDA is not doing resolution
  // change or waiting picture buffers, call FinishReset.
  void ResetTask();
  // This will schedule a ResetDoneTask() that will send the NotifyResetDone
  // callback, then set the decoder state to kResetting so that all intervening
  // tasks will drain.
  void FinishReset();
  void ResetDoneTask();

  // Device destruction task.
  void DestroyTask();

  // Start |device_poll_thread_|.
  bool StartDevicePoll();

  // Stop |device_poll_thread_|.
  bool StopDevicePoll();

  bool StopInputStream();
  bool StopOutputStream();

  void StartResolutionChange();
  void FinishResolutionChange();

  // Try to get output format and visible size, detected after parsing the
  // beginning of the stream. Sets |again| to true if more parsing is needed.
  // |visible_size| could be nullptr and ignored.
  bool GetFormatInfo(struct v4l2_format* format,
                     gfx::Size* visible_size,
                     bool* again);
  // Create output buffers for the given |format| and |visible_size|.
  bool CreateBuffersForFormat(const struct v4l2_format& format,
                              const gfx::Size& visible_size);

  // Try to get |visible_size|. Return visible size, or, if querying it is not
  // supported or produces invalid size, return |coded_size| instead.
  gfx::Size GetVisibleSize(const gfx::Size& coded_size);

  //
  // Device tasks, to be run on device_poll_thread_.
  //

  // The device task.
  void DevicePollTask(bool poll_device);

  //
  // Safe from any thread.
  //

  // Check whether a destroy is scheduled.
  bool IsDestroyPending();

  // Error notification (using PostTask() to child thread, if necessary).
  void NotifyError(Error error);

  // Set the decoder_state_ to kError and notify the client (if necessary).
  void SetErrorState(Error error);

  //
  // Other utility functions.  Called on decoder_thread_, unless
  // decoder_thread_ is not yet started, in which case the child thread can call
  // these (e.g. in Initialize() or Destroy()).
  //

  // Create the buffers we need.
  bool CreateInputBuffers();
  bool CreateOutputBuffers();

  // Destroy buffers.
  void DestroyInputBuffers();
  // In contrast to DestroyInputBuffers, which is called only on destruction,
  // we call DestroyOutputBuffers also during playback, on resolution change.
  // Even if anything fails along the way, we still want to go on and clean
  // up as much as possible, so return false if this happens, so that the
  // caller can error out on resolution change.
  bool DestroyOutputBuffers();

  // Set input and output formats before starting decode.
  bool SetupFormats();
  // Reset image processor and drop all processing frames.
  bool ResetImageProcessor();

  bool CreateImageProcessor();
  // Send a frame to the image processor to process. The index of decoder
  // output buffer is |output_buffer_index| and its id is |bitstream_buffer_id|.
  bool ProcessFrame(int32_t bitstream_buffer_id, V4L2ReadableBufferRef buf);

  // Send a buffer to the client.
  // |buffer_index| is the output buffer index of the buffer to be sent.
  // |bitstream_buffer_id| is the bitstream ID from which the buffer results.
  // |vda_buffer| is the output VDA buffer containing the decoded frame.
  // |frame| is the IP frame that will be sent to the client, if IP is used.
  void SendBufferToClient(size_t buffer_index,
                          int32_t bitstream_buffer_id,
                          V4L2ReadableBufferRef vda_buffer,
                          scoped_refptr<VideoFrame> frame = nullptr);

  //
  // Methods run on child thread.
  //

  // Send decoded pictures to PictureReady.
  void SendPictureReady();

  // Callback that indicates a picture has been cleared.
  void PictureCleared();

  // Image processor returns a processed |frame|. Its id is
  // |bitstream_buffer_id| and stored in |output_buffer_index| buffer of
  // image processor.
  void FrameProcessed(int32_t bitstream_buffer_id,
                      size_t output_buffer_index,
                      scoped_refptr<VideoFrame> frame);

  // Image processor notifies an error.
  void ImageProcessorError();

  // Our original calling task runner for the child thread.
  scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // Task runner Decode() and PictureReady() run on.
  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the decoder or
  // device worker threads back to the child thread.  Because the worker threads
  // are members of this class, any task running on those threads is guaranteed
  // that this object is still alive.  As a result, tasks posted from the child
  // thread to the decoder or device thread should use base::Unretained(this),
  // and tasks posted the other way should use |weak_this_|.
  base::WeakPtr<V4L2VideoDecodeAccelerator> weak_this_;

  // To expose client callbacks from VideoDecodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;
  base::WeakPtr<Client> client_;
  // Callbacks to |decode_client_| must be executed on |decode_task_runner_|.
  base::WeakPtr<Client> decode_client_;

  //
  // Decoder state, owned and operated by decoder_thread_.
  // Before decoder_thread_ has started, the decoder state is managed by
  // the child (main) thread.  After decoder_thread_ has started, the decoder
  // thread should be the only one managing these.
  //

  // This thread services tasks posted from the VDA API entry points by the
  // child thread and device service callbacks posted from the device thread.
  base::Thread decoder_thread_;
  // Decoder state machine state.
  State decoder_state_;

  // Waitable event signaled when the decoder is destroying.
  base::WaitableEvent destroy_pending_;

  Config::OutputMode output_mode_;

  // BitstreamBuffer we're presently reading.
  std::unique_ptr<BitstreamBufferRef> decoder_current_bitstream_buffer_;
  // The V4L2Device this class is operating upon.
  scoped_refptr<V4L2Device> device_;
  // FlushTask() and ResetTask() should not affect buffers that have been
  // queued afterwards.  For flushing or resetting the pipeline then, we will
  // delay these buffers until after the flush or reset completes.
  int decoder_delay_bitstream_buffer_id_;
  // We track the number of buffer decode tasks we have scheduled, since each
  // task execution should complete one buffer.  If we fall behind (due to
  // resource backpressure, etc.), we'll have to schedule more to catch up.
  int decoder_decode_buffer_tasks_scheduled_;

  // Are we flushing?
  bool decoder_flushing_;
  // True if VIDIOC_DECODER_CMD is supported.
  bool decoder_cmd_supported_;
  // True if flushing is waiting for last output buffer. After
  // VIDIOC_DECODER_CMD is sent to the driver, this flag will be set to true to
  // wait for the last output buffer. When this flag is true, flush done will
  // not be sent. After an output buffer that has the flag V4L2_BUF_FLAG_LAST is
  // received, this is set to false.
  bool flush_awaiting_last_output_buffer_;

  // Got a reset request while we were performing resolution change or waiting
  // picture buffers.
  bool reset_pending_;
  // Input queue for decoder_thread_: BitstreamBuffers in. Although the elements
  // in |decoder_input_queue_| is push()/pop() in queue order, this needs to be
  // base::circular_deque because we need to do random access in OnMemoryDump().
  base::circular_deque<std::unique_ptr<BitstreamBufferRef>>
      decoder_input_queue_;
  // For H264 decode, hardware requires that we send it frame-sized chunks.
  // We'll need to parse the stream.
  std::unique_ptr<H264Parser> decoder_h264_parser_;

  // Workaround for V4L2VideoDecodeAccelerator. This is created only if some
  // workaround is necessary for the V4L2VideoDecodeAccelerator.
  std::vector<std::unique_ptr<V4L2StatefulWorkaround>> workarounds_;

  // Set if the decoder has a pending incomplete frame in an input buffer.
  bool decoder_partial_frame_pending_;

  //
  // Hardware state and associated queues.  Since decoder_thread_ services
  // the hardware, decoder_thread_ owns these too.
  // output_buffer_map_ and output_planes_count_ are an
  // exception during the buffer (re)allocation sequence, when the
  // decoder_thread_ is blocked briefly while the Child thread manipulates
  // them.
  //

  V4L2WritableBufferRef current_input_buffer_;

  scoped_refptr<V4L2Queue> input_queue_;
  scoped_refptr<V4L2Queue> output_queue_;
  // Input buffers ready to be queued.
  base::queue<V4L2WritableBufferRef> input_ready_queue_;

  // Buffers that have been allocated but are awaiting an ImportBuffer
  // or AssignEGLImage event.
  std::map<int32_t, V4L2WritableBufferRef> output_wait_map_;
  // Bitstream IDs and VDA buffers currently being processed by the IP.
  std::queue<std::pair<int32_t, V4L2ReadableBufferRef>> buffers_at_ip_;
  // Keeps decoded buffers out of the free list until the client returns them.
  // First element is the VDA buffer, second is the (optional) IP buffer.
  std::map<int32_t, std::pair<V4L2ReadableBufferRef, scoped_refptr<VideoFrame>>>
      buffers_at_client_;
  // Queue of buffers that have been returned by the client, but which fence
  // hasn't been signaled yet. Keeps both the VDA and (optional) IP buffer.
  std::queue<
      std::pair<std::unique_ptr<gl::GLFenceEGL>,
                std::pair<V4L2ReadableBufferRef, scoped_refptr<VideoFrame>>>>
      buffers_awaiting_fence_;

  // Mapping of int index to output buffer record.
  std::vector<OutputRecord> output_buffer_map_;
  // Required size of DPB for decoding.
  int output_dpb_size_;

  // Number of planes (i.e. separate memory buffers) for output.
  size_t output_planes_count_;

  // Pictures that are ready but not sent to PictureReady yet.
  base::queue<PictureRecord> pending_picture_ready_;

  // The number of pictures that are sent to PictureReady and will be cleared.
  int picture_clearing_count_;

  // Output picture coded size.
  gfx::Size coded_size_;

  // Output picture visible size.
  gfx::Size visible_size_;

  //
  // The device polling thread handles notifications of V4L2 device changes.
  //

  // The thread.
  base::Thread device_poll_thread_;

  //
  // Other state, held by the child (main) thread.
  //

  // EGL state
  EGLDisplay egl_display_;

  // Callback to get current GLContext.
  GetGLContextCallback get_gl_context_cb_;
  // Callback to set the correct gl context.
  MakeGLContextCurrentCallback make_context_current_cb_;

  // The codec we'll be decoding for.
  VideoCodecProfile video_profile_;
  // Chosen input format for video_profile_.
  uint32_t input_format_fourcc_;
  // Chosen output format.
  uint32_t output_format_fourcc_;

  // Image processor device, if one is in use.
  scoped_refptr<V4L2Device> image_processor_device_;
  // Image processor. Accessed on |decoder_thread_|.
  std::unique_ptr<ImageProcessor> image_processor_;

  // The V4L2Device EGLImage is created from.
  scoped_refptr<V4L2Device> egl_image_device_;
  // The format of EGLImage.
  uint32_t egl_image_format_fourcc_;
  // The logical dimensions of EGLImage buffer in pixels.
  gfx::Size egl_image_size_;
  // Number of planes for EGLImage.
  size_t egl_image_planes_count_;

  // Input format V4L2 fourccs this class supports.
  static const uint32_t supported_input_fourccs_[];

  // The WeakPtrFactory for |weak_this_|.
  base::WeakPtrFactory<V4L2VideoDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2VideoDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODE_ACCELERATOR_H_
