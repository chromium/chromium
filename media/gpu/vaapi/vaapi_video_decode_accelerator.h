// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VideoDecoderAccelerator
// that utilizes hardware video decoder present on Intel CPUs.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/containers/small_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "media/base/bitstream_buffer.h"
#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/vaapi_picture_factory.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"

namespace gl {
class GLImage;
}

namespace media {

class AcceleratedVideoDecoder;
template <typename T>
class ScopedID;
class VaapiVideoDecoderDelegate;
class VaapiPicture;

// Class to provide video decode acceleration for Intel systems with hardware
// support for it, and on which libva is available.
// Decoding tasks are performed in a separate decoding thread.
//
// Threading/life-cycle: this object is created & destroyed on the GPU
// ChildThread.  A few methods on it are called on the decoder thread which is
// stopped during |this->Destroy()|, so any tasks posted to the decoder thread
// can assume |*this| is still alive.  See |weak_this_| below for more details.
class MEDIA_GPU_EXPORT VaapiVideoDecodeAccelerator
    : public VideoDecodeAccelerator,
      public DecodeSurfaceHandler<VASurface>,
      public base::trace_event::MemoryDumpProvider {
 public:
  VaapiVideoDecodeAccelerator(
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb);

  VaapiVideoDecodeAccelerator(const VaapiVideoDecodeAccelerator&) = delete;
  VaapiVideoDecodeAccelerator& operator=(const VaapiVideoDecodeAccelerator&) =
      delete;

  ~VaapiVideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation.
  bool Initialize(const Config& config, Client* client) override;
  void Decode(BitstreamBuffer bitstream_buffer) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              int32_t bitstream_id) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
#if BUILDFLAG(IS_OZONE)
  void ImportBufferForPicture(
      int32_t picture_buffer_id,
      VideoPixelFormat pixel_format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;
#endif
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateSequence(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner)
      override;

  static VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles();

  // DecodeSurfaceHandler implementation.
  scoped_refptr<VASurface> CreateSurface() override;
  void SurfaceReady(scoped_refptr<VASurface> va_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class VaapiVideoDecodeAcceleratorTest;

  // An input buffer with id provided by the client and awaiting consumption.
  class InputBuffer;
  // A self-cleaning VASurfaceID.
  using ScopedVASurfaceID = ScopedID<VASurfaceID>;

  // Notify the client that an error has occurred and decoding cannot continue.
  void NotifyError(Error error);
  void NotifyStatus(VaapiStatus status);

  // Queue a input buffer for decode.
  void QueueInputBuffer(scoped_refptr<DecoderBuffer> buffer,
                        int32_t bitstream_id);

  // Gets a new |current_input_buffer_| from |input_buffers_| and sets it up in
  // |decoder_|. This method will sleep if no |input_buffers_| are available.
  // Returns true if a new buffer has been set up, false if an early exit has
  // been requested (due to initiated reset/flush/destroy).
  bool GetCurrInputBuffer_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Signals the client that |curr_input_buffer_| has been read and can be
  // returned. Will also release the mapping.
  void ReturnCurrInputBuffer_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Waits for more surfaces to become available. Returns true once they do or
  // false if an early exit has been requested (due to an initiated
  // reset/flush/destroy).
  bool WaitForSurfaces_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Continue decoding given input buffers and sleep waiting for input/output
  // as needed. Will exit if a new set of surfaces or reset/flush/destroy
  // is requested.
  void DecodeTask();

  // Scheduled after receiving a flush request and executed after the current
  // decoding task finishes decoding pending inputs. Makes the decoder return
  // all remaining output pictures and puts it in an idle state, ready
  // to resume if needed and schedules a FinishFlush.
  void FlushTask();

  // Scheduled by the FlushTask after decoder is flushed to put VAVDA into idle
  // state and notify the client that flushing has been finished.
  void FinishFlush();

  // Scheduled after receiving a reset request and executed after the current
  // decoding task finishes decoding the current frame. Puts the decoder into
  // an idle state, ready to resume if needed, discarding decoded but not yet
  // outputted pictures (decoder keeps ownership of their associated picture
  // buffers). Schedules a FinishReset afterwards.
  void ResetTask();

  // Scheduled by ResetTask after it's done putting VAVDA into an idle state.
  // Drops remaining input buffers and notifies the client that reset has been
  // finished.
  void FinishReset();

  // Helper for Destroy(), doing all the actual work except for deleting self.
  void Cleanup();

  // Get a usable framebuffer configuration for use in binding textures
  // or return false on failure.
  bool InitializeFBConfig();

  // Callback to be executed once we have a |va_surface| to be output and an
  // available VaapiPicture in |available_picture_buffers_| for output. Puts
  // contents of |va_surface| into the latter, releases the surface and passes
  // the resulting picture to |client_| along with |visible_rect|.
  void OutputPicture(scoped_refptr<VASurface> va_surface,
                     int32_t input_id,
                     gfx::Rect visible_rect,
                     const VideoColorSpace& picture_color_space);

  // Try to OutputPicture() if we have both a ready surface and picture.
  void TryOutputPicture();

  // Called when a VASurface is no longer in use by |decoder_| nor |client_|.
  // Returns it to |available_va_surfaces_|. |va_surface_id| is not used but it
  // must be here to bind this method as VASurface::ReleaseCB.
  void RecycleVASurface(std::unique_ptr<ScopedVASurfaceID> va_surface,
                        VASurfaceID va_surface_id);

  // Request a new set of |num_pics| PictureBuffers to be allocated by
  // |client_|. Up to |num_reference_frames| out of |num_pics_| might be needed
  // by |decoder_|.
  void InitiateSurfaceSetChange(size_t num_pics,
                                gfx::Size size,
                                size_t num_reference_frames,
                                const gfx::Rect& visible_rect);

  // Check if the surfaces have been released or post ourselves for later.
  void TryFinishSurfaceSetChange();

  // Different modes of internal buffer allocations.
  enum class BufferAllocationMode {
    // Only using |client_|s provided PictureBuffers, none internal.
    kNone,

    // Using a reduced amount of |client_|s provided PictureBuffers and
    // |decoder_|s GetNumReferenceFrames() internallly.
    kSuperReduced,

    // Similar to kSuperReduced, but we have to increase slightly the amount of
    // PictureBuffers allocated for the |client_|.
    kReduced,

    // VaapiVideoDecodeAccelerator can work with this mode on all platforms.
    // Using |client_|s provided PictureBuffers and as many internally
    // allocated.
    kNormal,
  };

  // Decides the concrete buffer allocation mode, depending on the hardware
  // platform and other parameters.
  BufferAllocationMode DecideBufferAllocationMode();
  bool IsBufferAllocationModeReducedOrSuperReduced() const;

  // VAVDA state.
  enum State {
    // Initialize() not called yet or failed.
    kUninitialized,
    // DecodeTask running.
    kDecoding,
    // Resetting, waiting for decoder to finish current task and cleanup.
    kResetting,
    // Idle, decoder in state ready to start/resume decoding.
    kIdle,
    // Destroying, waiting for the decoder to finish current task.
    kDestroying,
  };

  base::Lock lock_;
  State state_ GUARDED_BY(lock_);
  // Only used on |task_runner_|.
  Config::OutputMode output_mode_;

  // Queue of available InputBuffers.
  base::queue<std::unique_ptr<InputBuffer>> input_buffers_ GUARDED_BY(lock_);
  // Signalled when input buffers are queued onto |input_buffers_| queue.
  base::ConditionVariable input_ready_;

  // Current input buffer at decoder. Only used on |decoder_thread_task_runner_|
  std::unique_ptr<InputBuffer> curr_input_buffer_;

  // Only used on |task_runner_|.
  std::unique_ptr<VaapiPictureFactory> vaapi_picture_factory_;

  // The following variables are constructed/initialized in Initialize() when
  // the codec information is received. |vaapi_wrapper_| is thread safe.
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  // Only used on |decoder_thread_task_runner_|.
  std::unique_ptr<AcceleratedVideoDecoder> decoder_;
  // TODO(crbug.com/1022246): Instead of having the raw pointer here, getting
  // the pointer from AcceleratedVideoDecoder.
  raw_ptr<VaapiVideoDecoderDelegate> decoder_delegate_ = nullptr;

  // Filled in during Initialize().
  BufferAllocationMode buffer_allocation_mode_;

  // VaapiWrapper for VPP (Video Post Processing). This is used for copying
  // from a decoded surface to a surface bound to client's PictureBuffer.
  scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper_;

  // All allocated VaapiPictures, regardless of their current state. Pictures
  // are allocated at AssignPictureBuffers() and are kept until dtor or
  // TryFinishSurfaceSetChange(). Comes after |vaapi_wrapper_| to ensure all
  // pictures are destroyed before this is destroyed.
  base::small_map<std::map<int32_t, std::unique_ptr<VaapiPicture>>> pictures_
      GUARDED_BY(lock_);
  // List of PictureBuffer ids available to be sent to |client_| via
  // OutputPicture() (|client_| returns them via ReusePictureBuffer()).
  std::list<int32_t> available_picture_buffers_ GUARDED_BY(lock_);

  // VASurfaces available and that can be passed to |decoder_| for its use upon
  // CreateSurface() request (and then returned via RecycleVASurface()).
  std::list<std::unique_ptr<ScopedVASurfaceID>> available_va_surfaces_
      GUARDED_BY(lock_);
  // Signalled when output surfaces are queued into |available_va_surfaces_|.
  base::ConditionVariable surfaces_available_;
  // VASurfaceIDs format, filled in when created.
  unsigned int va_surface_format_;

  // Pending output requests from the decoder. When it indicates that we should
  // output a surface and we have an available Picture (i.e. texture) ready
  // to use, we'll execute the callback passing the Picture. The callback
  // will put the contents of the surface into the picture and return it to
  // the client, releasing the surface as well.
  // If we don't have any available |pictures_| at the time when the decoder
  // requests output, we'll store the request in this queue for later and run it
  // once the client gives us more textures via ReusePictureBuffer().
  // Only used on |task_runner_|.
  base::queue<base::OnceClosure> pending_output_cbs_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the decoder
  // thread back to the ChildThread.  Because the decoder thread is a member of
  // this class, any task running on the decoder thread is guaranteed that this
  // object is still alive.  As a result, tasks posted from ChildThread to
  // decoder thread should use base::Unretained(this), and tasks posted from the
  // decoder thread to the ChildThread should use |weak_this_|.
  base::WeakPtr<VaapiVideoDecodeAccelerator> weak_this_;

  // Callback used to recycle VASurfaces. Only used on |task_runner_|.
  base::RepeatingCallback<void(std::unique_ptr<ScopedVASurfaceID>, VASurfaceID)>
      va_surface_recycle_cb_;

  // To expose client callbacks from VideoDecodeAccelerator. Used only on
  // |task_runner_|.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;
  base::WeakPtr<Client> client_;

  // ChildThread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::Thread decoder_thread_;
  // Use this to post tasks to |decoder_thread_| instead of
  // |decoder_thread_.task_runner()| because the latter will be NULL once
  // |decoder_thread_.Stop()| returns.
  scoped_refptr<base::SingleThreadTaskRunner> decoder_thread_task_runner_;

  // Whether we are waiting for any |pending_output_cbs_| to be run before
  // NotifyingFlushDone. Only used on |task_runner_|.
  bool finish_flush_pending_;

  // Decoder requested a new surface set and we are waiting for all the surfaces
  // to be returned before we can free them. Only used on |task_runner_|.
  bool awaiting_va_surfaces_recycle_;

  // Last requested number/resolution/visible rectangle of output
  // PictureBuffers.
  size_t requested_num_pics_;
  gfx::Size requested_pic_size_;
  gfx::Rect requested_visible_rect_;
  // Potential extra PictureBuffers to request, used only on
  // BufferAllocationMode::kNone, see DecideBufferAllocationMode().
  size_t num_extra_pics_ = 0;

  // Max number of reference frames needed by |decoder_|. Only used on
  // |task_runner_| and when in BufferAllocationMode::kNone.
  size_t requested_num_reference_frames_;
  size_t previously_requested_num_reference_frames_;

  // The video stream's profile.
  VideoCodecProfile profile_;

  // Callback to make GL context current.
  MakeGLContextCurrentCallback make_context_current_cb_;

  // Callback to bind a GLImage to a given texture.
  BindGLImageCallback bind_image_cb_;

  // The WeakPtrFactory for |weak_this_|.
  base::WeakPtrFactory<VaapiVideoDecodeAccelerator> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODE_ACCELERATOR_H_
