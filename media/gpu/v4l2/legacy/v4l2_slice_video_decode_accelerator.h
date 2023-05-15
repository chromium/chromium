// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_LEGACY_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_V4L2_LEGACY_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_

#include <linux/videodev2.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/service/shared_image/gl_image_native_pixmap.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/vp8_decoder.h"
#include "media/gpu/vp9_decoder.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gl/gl_fence_egl.h"

namespace media {

class V4L2DecodeSurface;
class ImageProcessor;

// An implementation of VideoDecodeAccelerator that utilizes the V4L2 slice
// level codec API for decoding. The slice level API provides only a low-level
// decoding functionality and requires userspace to provide support for parsing
// the input stream and managing decoder state across frames.
class MEDIA_GPU_EXPORT V4L2SliceVideoDecodeAccelerator
    : public VideoDecodeAccelerator,
      public V4L2DecodeSurfaceHandler,
      public base::trace_event::MemoryDumpProvider {
 public:
  V4L2SliceVideoDecodeAccelerator(
      scoped_refptr<V4L2Device> device,
      EGLDisplay egl_display,
      const BindGLImageCallback& bind_image_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb);

  V4L2SliceVideoDecodeAccelerator(const V4L2SliceVideoDecodeAccelerator&) =
      delete;
  V4L2SliceVideoDecodeAccelerator& operator=(
      const V4L2SliceVideoDecodeAccelerator&) = delete;

  ~V4L2SliceVideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation.
  bool Initialize(const Config& config, Client* client) override;
  void Decode(BitstreamBuffer bitstream_buffer) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              int32_t bitstream_id) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ImportBufferForPicture(
      int32_t picture_buffer_id,
      VideoPixelFormat pixel_format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateSequence(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner)
      override;

  static VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:

  // Record for output buffers.
  struct OutputRecord {
    OutputRecord();
    OutputRecord(OutputRecord&&);
    ~OutputRecord();

    // Final output frame (i.e. processed if an image processor is used).
    // Used only when OutputMode is IMPORT.
    scoped_refptr<VideoFrame> output_frame;

    // The members below are referring to the displayed buffer - this may
    // be the decoder buffer, or the IP buffer if an IP is in use. In this case,
    // ip_buffer_index contains the entry number of the IP buffer.
    int32_t picture_id;
    GLuint client_texture_id;
    GLuint texture_id;
    bool cleared;
    size_t num_times_sent_to_client;

    bool at_client() const { return num_times_sent_to_client > 0; }
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

  static scoped_refptr<gpu::GLImageNativePixmap> CreateGLImage(
      const gfx::Size& size,
      const Fourcc fourcc,
      gfx::NativePixmapHandle handle,
      GLenum target,
      GLuint texture_id);

  //
  // Below methods are used by accelerator implementations.
  //
  // V4L2DecodeSurfaceHandler implementation.

  // Release surfaces awaiting for their fence to be signaled.
  void CheckGLFences();

  scoped_refptr<V4L2DecodeSurface> CreateSurface() override;
  // SurfaceReady() uses |decoder_display_queue_| to guarantee that decoding
  // of |dec_surface| happens in order.
  void SurfaceReady(scoped_refptr<V4L2DecodeSurface> dec_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& /* color_space */) override;
  bool SubmitSlice(V4L2DecodeSurface* dec_surface,
                   const uint8_t* data,
                   size_t size) override;
  void DecodeSurface(scoped_refptr<V4L2DecodeSurface> dec_surface) override;

  //
  // Internal methods of this class.
  //
  // Recycle V4L2 output buffer with |index|. Used as surface release callback.
  void ReuseOutputBuffer(V4L2ReadableBufferRef buffer);

  // Dequeue any V4L2 buffers available and process.
  void Dequeue();

  // Set input and output formats in hardware.
  bool SetupFormats();
  // Reset image processor and drop all processing frames.
  bool ResetImageProcessor();

  bool CreateImageProcessor();

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

  // Use buffer backed by |handle| for the OutputRecord associated with
  // |picture_buffer_id|. |handle| does not need to be valid if we are in
  // ALLOCATE mode and using an image processor.
  void ImportBufferForPictureTask(int32_t picture_buffer_id,
                                  gfx::NativePixmapHandle handle);

  // Check that |planes| and |dmabuf_fds| are valid in import mode and call
  // ImportBufferForPictureTask.
  void ImportBufferForPictureForImportTask(int32_t picture_buffer_id,
                                           VideoPixelFormat pixel_format,
                                           gfx::NativePixmapHandle handle);

  // Create a GLImage on |gl_device| for the buffer associated with V4L2
  // |buffer_index| and |picture_buffer_id|, backed by |handle|.
  // The GLImage will be associated |client_texture_id| in gles2 decoder and is
  // of format |fourcc|. |visible_size| is the size in pixels that the GL device
  // will be able to see.
  void CreateGLImageFor(scoped_refptr<V4L2Device> gl_device,
                        size_t buffer_index,
                        int32_t picture_buffer_id,
                        gfx::NativePixmapHandle handle,
                        GLuint client_texture_id,
                        GLuint texture_id,
                        const gfx::Size& visible_size,
                        const Fourcc fourcc);

  // Performed on decoder_thread_ as a consequence of poll() on decoder_thread_
  // returning an event. Typically this means that there are output or capture
  // buffers that are ready to be dequeued.
  // |event| is set to true by the poller if a V4L2 event should be dequeued
  // using VIDIOC_DQEVENT, but this should never happen for the slice API.
  void ServiceDeviceTask(bool event);

  // Attempt to start/stop the V4L2 device poller.
  bool StartDevicePoll();
  bool StopDevicePoll();
  void OnPollError();

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
                     scoped_refptr<V4L2DecodeSurface> dec_surface);

  // Goes over the |decoder_display_queue_| and sends all buffers from the
  // front of the queue that are already decoded to the client, in order.
  void TryOutputSurfaces();

  // Send decoded pictures to PictureReady.
  void SendPictureReady();

  // Callback that indicates a picture has been cleared.
  void PictureCleared();

  // Returns the number of OutputRecords at client/device. This is used to
  // compute values reported for chrome://tracing.
  size_t GetNumOfOutputRecordsAtClient() const;
  size_t GetNumOfOutputRecordsAtDevice() const;

  // Image processor notifies an error.
  void ImageProcessorError();

  bool ProcessFrame(V4L2ReadableBufferRef buffer,
                    scoped_refptr<V4L2DecodeSurface>);
  void FrameProcessed(scoped_refptr<V4L2DecodeSurface> surface,
                      size_t ip_buffer_index,
                      scoped_refptr<VideoFrame> frame);

  // Returns whether |profile| is supported by a v4l2 decoder driver.
  bool IsSupportedProfile(VideoCodecProfile profile);

  // TODO(crbug.com/1109312): some pages with lots of small videos are causing
  // crashes, so limit the number of simultaneous decoder instances for now.
  // |num_instances_| tracks the number of simultaneous decoders.
  // |can_use_decoder_| is true iff we haven't reached the maximum number of
  // instances at the time this decoder is created.
  static constexpr int kMaxNumOfInstances = 10;
  static base::AtomicRefCount num_instances_;
  const bool can_use_decoder_;

  // VideoCodecProfiles supported by a v4l2 decoder driver.
  std::vector<VideoCodecProfile> supported_profiles_;

  size_t output_planes_count_;

  // GPU Child thread task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // Task runner Decode() and PictureReady() run on.
  scoped_refptr<base::SequencedTaskRunner> decode_task_runner_;

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

  scoped_refptr<V4L2Queue> input_queue_;
  scoped_refptr<V4L2Queue> output_queue_;
  // Buffers that have been allocated but are awaiting an ImportBuffer
  // or AssignDmabufs event.
  std::map<int32_t, V4L2WritableBufferRef> output_wait_map_;
  // Mapping of int index to an output buffer record.
  std::vector<OutputRecord> output_buffer_map_;
  // Maps a decoded buffer index to the output record of the buffer to be
  // displayed. Both indices are the same in most cases, except when we use
  // an image processor in ALLOCATE mode in which case the index of the IP
  // buffer may not match the one of the decoder.
  std::map<int32_t, int32_t> decoded_buffer_map_;
  // FIFO queue of requests.
  std::queue<base::ScopedFD> requests_;

  VideoCodecProfile video_profile_;
  uint32_t input_format_fourcc_;
  absl::optional<Fourcc> output_format_fourcc_;
  gfx::Size coded_size_;

  struct BitstreamBufferRef;
  // Input queue of stream buffers coming from the client. Although the elements
  // in |decoder_input_queue_| is push()/pop() in queue order, this needs to be
  // base::circular_deque because we need to do random access in OnMemoryDump().
  base::circular_deque<std::unique_ptr<BitstreamBufferRef>>
      decoder_input_queue_;
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

  // The visible rect of the frames in the output queue.
  gfx::Rect visible_rect_;

  // Surfaces queued to device to keep references to them while decoded.
  std::queue<scoped_refptr<V4L2DecodeSurface>> surfaces_at_device_;

  // Surfaces currently being processed by IP.
  std::queue<std::pair<scoped_refptr<V4L2DecodeSurface>, V4L2ReadableBufferRef>>
      surfaces_at_ip_;

  // Surfaces sent to client to keep references to them while displayed.
  using V4L2DecodeSurfaceByPictureBufferId =
      std::map<int32_t, scoped_refptr<V4L2DecodeSurface>>;
  V4L2DecodeSurfaceByPictureBufferId surfaces_at_display_;

  // Queue of surfaces that have been returned by the client, but which fence
  // hasn't been signaled yet.
  std::queue<std::pair<std::unique_ptr<gl::GLFenceEGL>,
                       scoped_refptr<V4L2DecodeSurface>>>
      surfaces_awaiting_fence_;

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

  // Image processor device, if one is in use.
  scoped_refptr<V4L2Device> image_processor_device_;
  // Image processor. Accessed on |decoder_thread_|.
  std::unique_ptr<ImageProcessor> image_processor_;

  // The format of GLImage.
  absl::optional<Fourcc> gl_image_format_fourcc_;
  // The logical dimensions of GLImage buffer in pixels.
  gfx::Size gl_image_size_;
  // Number of planes for GLImage.
  size_t gl_image_planes_count_;

  // Reference to request queue to get free requests.
  raw_ptr<V4L2RequestsQueue> requests_queue_;

  // The WeakPtrFactory for |weak_this_|.
  base::WeakPtrFactory<V4L2SliceVideoDecodeAccelerator> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_LEGACY_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_
