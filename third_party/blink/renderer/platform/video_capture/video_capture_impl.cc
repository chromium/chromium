// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Notes about usage of this object by VideoCaptureImplManager.
//
// VideoCaptureImplManager access this object by using a Unretained()
// binding and tasks on the IO thread. It is then important that
// VideoCaptureImpl never post task to itself. All operations must be
// synchronous.

#include "third_party/blink/renderer/platform/video_capture/video_capture_impl.h"

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

constexpr int kMaxFirstFrameLogs = 5;

const base::Feature kTimeoutHangingVideoCaptureStarts{
    "TimeoutHangingVideoCaptureStarts", base::FEATURE_ENABLED_BY_DEFAULT};

using VideoFrameBufferHandleType = media::mojom::blink::VideoBufferHandle::Tag;

// A collection of all types of handles that we use to reference a camera buffer
// backed with GpuMemoryBuffer.
struct GpuMemoryBufferResources {
  explicit GpuMemoryBufferResources(gfx::GpuMemoryBufferHandle handle)
      : gpu_memory_buffer_handle(std::move(handle)) {}
  // Stores the GpuMemoryBufferHandle when a new buffer is first registered.
  // |gpu_memory_buffer_handle| is converted to |gpu_memory_buffer| below when
  // the camera frame is ready for the first time.
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
  // The GpuMemoryBuffer backing the camera frame.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
  // The SharedImage created from |gpu_memory_buffer|.
  gpu::Mailbox mailbox;
  // The release sync token for |mailbox|.
  gpu::SyncToken release_sync_token;
};

struct VideoCaptureImpl::BufferContext
    : public base::RefCountedThreadSafe<BufferContext> {
 public:
  BufferContext(media::mojom::blink::VideoBufferHandlePtr buffer_handle,
                scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
      : buffer_type_(buffer_handle->which()),
        media_task_runner_(media_task_runner) {
    switch (buffer_type_) {
      case VideoFrameBufferHandleType::SHARED_BUFFER_HANDLE:
        InitializeFromSharedMemory(
            std::move(buffer_handle->get_shared_buffer_handle()));
        break;
      case VideoFrameBufferHandleType::READ_ONLY_SHMEM_REGION:
        InitializeFromReadOnlyShmemRegion(
            std::move(buffer_handle->get_read_only_shmem_region()));
        break;
      case VideoFrameBufferHandleType::SHARED_MEMORY_VIA_RAW_FILE_DESCRIPTOR:
        NOTREACHED();
        break;
      case VideoFrameBufferHandleType::MAILBOX_HANDLES:
        InitializeFromMailbox(std::move(buffer_handle->get_mailbox_handles()));
        break;
      case VideoFrameBufferHandleType::GPU_MEMORY_BUFFER_HANDLE:
#if !defined(OS_MAC)
        // On macOS, an IOSurfaces passed as a GpuMemoryBufferHandle can be
        // used by both hardware and software paths.
        // https://crbug.com/1125879
        CHECK(media_task_runner_);
#endif
        InitializeFromGpuMemoryBufferHandle(
            std::move(buffer_handle->get_gpu_memory_buffer_handle()));
        break;
    }
  }

  VideoFrameBufferHandleType buffer_type() const { return buffer_type_; }
  const uint8_t* data() const { return data_; }
  size_t data_size() const { return data_size_; }
  const Vector<gpu::MailboxHolder>& mailbox_holders() const {
    return mailbox_holders_;
  }

  gfx::GpuMemoryBufferHandle TakeGpuMemoryBufferHandle() {
    return std::move(gmb_resources_->gpu_memory_buffer_handle);
  }
  void SetGpuMemoryBuffer(
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer) {
    gmb_resources_->gpu_memory_buffer = std::move(gpu_memory_buffer);
  }
  gfx::GpuMemoryBuffer* GetGpuMemoryBuffer() {
    return gmb_resources_->gpu_memory_buffer.get();
  }

  // Creates SharedImage mailboxes for |gpu_memory_buffer_handle_| and wraps the
  // mailboxes with the buffer handles in a DMA-buf VideoFrame.  The consumer of
  // the VideoFrame can access the data either through mailboxes (e.g. display)
  // or through the DMA-buf FDs (e.g. video encoder).
  static void BindBufferToTextureOnMediaThread(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      scoped_refptr<BufferContext> buffer_context,
      media::mojom::blink::VideoFrameInfoPtr info,
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
      scoped_refptr<media::VideoFrame> frame,
      base::OnceCallback<void(media::mojom::blink::VideoFrameInfoPtr,
                              scoped_refptr<media::VideoFrame>,
                              scoped_refptr<BufferContext>)> on_texture_bound,
      base::OnceCallback<void()> on_gpu_context_lost) {
    DCHECK(gpu_factories);
    DCHECK(buffer_context->media_task_runner_->BelongsToCurrentThread());
    DCHECK_EQ(info->pixel_format, media::PIXEL_FORMAT_NV12);

    bool should_recreate_shared_image = false;
    if (gpu_factories != buffer_context->gpu_factories_) {
      DVLOG(1) << "GPU context changed; re-creating SharedImage objects";
      buffer_context->gpu_factories_ = gpu_factories;
      should_recreate_shared_image = true;
    }

    // Create GPU texture and bind GpuMemoryBuffer to the texture.
    auto* sii = buffer_context->gpu_factories_->SharedImageInterface();
    if (!sii) {
      DVLOG(1) << "GPU context lost";
      std::move(on_gpu_context_lost).Run();
      std::move(on_texture_bound)
          .Run(std::move(info), std::move(frame), std::move(buffer_context));
      return;
    }
    // Don't check VideoFrameOutputFormat until we ensure the context has not
    // been lost (if it is lost, then the format will be UNKNOWN).
    DCHECK_EQ(
        buffer_context->gpu_factories_->VideoFrameOutputFormat(
            info->pixel_format),
        media::GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB);
    unsigned texture_target =
        buffer_context->gpu_factories_->ImageTextureTarget(
            gpu_memory_buffer->GetFormat());
    if (should_recreate_shared_image ||
        buffer_context->gmb_resources_->mailbox.IsZero()) {
      uint32_t usage =
          gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_RASTER |
          gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT |
          gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX;
      buffer_context->gmb_resources_->mailbox = sii->CreateSharedImage(
          gpu_memory_buffer.get(),
          buffer_context->gpu_factories_->GpuMemoryBufferManager(),
          *(info->color_space), kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
          usage);
    } else {
      sii->UpdateSharedImage(buffer_context->gmb_resources_->release_sync_token,
                             buffer_context->gmb_resources_->mailbox);
    }
    gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();
    CHECK(!buffer_context->gmb_resources_->mailbox.IsZero());
    CHECK(buffer_context->gmb_resources_->mailbox.IsSharedImage());
    gpu::MailboxHolder mailbox_holder_array[media::VideoFrame::kMaxPlanes];
    mailbox_holder_array[0] = gpu::MailboxHolder(
        buffer_context->gmb_resources_->mailbox, sync_token, texture_target);

    const auto gmb_size = gpu_memory_buffer->GetSize();
    frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
        gfx::Rect(info->visible_rect), gmb_size, std::move(gpu_memory_buffer),
        mailbox_holder_array,
        base::BindOnce(&BufferContext::MailboxHolderReleased, buffer_context),
        info->timestamp);
    frame->metadata()->allow_overlay = true;
    frame->metadata()->read_lock_fences_enabled = true;

    std::move(on_texture_bound)
        .Run(std::move(info), std::move(frame), std::move(buffer_context));
  }

  static void MailboxHolderReleased(scoped_refptr<BufferContext> buffer_context,
                                    const gpu::SyncToken& release_sync_token) {
    if (!buffer_context->media_task_runner_->BelongsToCurrentThread()) {
      buffer_context->media_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&BufferContext::MailboxHolderReleased,
                                    buffer_context, release_sync_token));
      return;
    }
    buffer_context->gmb_resources_->release_sync_token = release_sync_token;
  }

  static void DestroyTextureOnMediaThread(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      gpu::Mailbox mailbox,
      gpu::SyncToken release_sync_token) {
    if (!mailbox.IsZero()) {
      auto* sii = gpu_factories->SharedImageInterface();
      if (!sii)
        return;
      sii->DestroySharedImage(release_sync_token, mailbox);
    }
  }

 private:
  void InitializeFromSharedMemory(mojo::ScopedSharedBufferHandle handle) {
    DCHECK(handle.is_valid());
    base::UnsafeSharedMemoryRegion region =
        mojo::UnwrapUnsafeSharedMemoryRegion(std::move(handle));
    if (!region.IsValid()) {
      DLOG(ERROR) << "Unwrapping shared memory failed.";
      return;
    }
    writable_mapping_ = region.Map();
    if (!writable_mapping_.IsValid()) {
      DLOG(ERROR) << "Mapping shared memory failed.";
      return;
    }
    data_ = writable_mapping_.GetMemoryAsSpan<uint8_t>().data();
    data_size_ = writable_mapping_.size();
  }

  void InitializeFromReadOnlyShmemRegion(
      base::ReadOnlySharedMemoryRegion region) {
    DCHECK(region.IsValid());
    read_only_mapping_ = region.Map();
    DCHECK(read_only_mapping_.IsValid());
    data_ = read_only_mapping_.GetMemoryAsSpan<uint8_t>().data();
    data_size_ = read_only_mapping_.size();
  }

  void InitializeFromMailbox(
      media::mojom::blink::MailboxBufferHandleSetPtr mailbox_handles) {
    DCHECK_EQ(media::VideoFrame::kMaxPlanes,
              mailbox_handles->mailbox_holder.size());
    mailbox_holders_ = std::move(mailbox_handles->mailbox_holder);
  }

  void InitializeFromGpuMemoryBufferHandle(
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
    gmb_resources_ = std::make_unique<GpuMemoryBufferResources>(
        std::move(gpu_memory_buffer_handle));
  }

  friend class base::RefCountedThreadSafe<BufferContext>;
  virtual ~BufferContext() {
    if (gmb_resources_ && gmb_resources_->mailbox.IsSharedImage()) {
      media_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&BufferContext::DestroyTextureOnMediaThread,
                                    gpu_factories_, gmb_resources_->mailbox,
                                    gmb_resources_->release_sync_token));
    }
  }

  VideoFrameBufferHandleType buffer_type_;

  // Only valid for |buffer_type_ == SHARED_BUFFER_HANDLE|.
  base::WritableSharedMemoryMapping writable_mapping_;

  // Only valid for |buffer_type_ == READ_ONLY_SHMEM_REGION|.
  base::ReadOnlySharedMemoryMapping read_only_mapping_;

  // These point into one of the above mappings, which hold the mapping open for
  // the lifetime of this object.
  const uint8_t* data_ = nullptr;
  size_t data_size_ = 0;

  // Only valid for |buffer_type_ == MAILBOX_HANDLES|.
  Vector<gpu::MailboxHolder> mailbox_holders_;

  // The following is for |buffer_type == GPU_MEMORY_BUFFER_HANDLE|.

  // Uses to create SharedImage from |gpu_memory_buffer_|.
  media::GpuVideoAcceleratorFactories* gpu_factories_;
  // The task runner that |gpu_factories_| runs on.
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  std::unique_ptr<GpuMemoryBufferResources> gmb_resources_;

  DISALLOW_COPY_AND_ASSIGN(BufferContext);
};

// Information about a video capture client of ours.
struct VideoCaptureImpl::ClientInfo {
  ClientInfo() = default;

  ClientInfo(const ClientInfo& other) = default;

  ~ClientInfo() = default;

  media::VideoCaptureParams params;

  VideoCaptureStateUpdateCB state_update_cb;

  VideoCaptureDeliverFrameCB deliver_frame_cb;
};

VideoCaptureImpl::VideoCaptureImpl(
    media::VideoCaptureSessionId session_id,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : device_id_(session_id),
      session_id_(session_id),
      video_capture_host_for_testing_(nullptr),
      state_(blink::VIDEO_CAPTURE_STATE_STOPPED),
      main_task_runner_(std::move(main_task_runner)),
      gpu_memory_buffer_support_(new gpu::GpuMemoryBufferSupport()) {
  CHECK(!session_id.is_empty());
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DETACH_FROM_THREAD(io_thread_checker_);

  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      pending_video_capture_host_.InitWithNewPipeAndPassReceiver());

  gpu_factories_ = Platform::Current()->GetGpuFactories();
  if (gpu_factories_) {
    media_task_runner_ = gpu_factories_->GetTaskRunner();
  }
}

void VideoCaptureImpl::OnGpuContextLost(
    base::WeakPtr<VideoCaptureImpl> video_capture_impl) {
  // Called on the main task runner.
  auto* gpu_factories = Platform::Current()->GetGpuFactories();
  Platform::Current()->GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureImpl::SetGpuFactoriesHandleOnIOTaskRunner,
                     video_capture_impl, gpu_factories));
}

void VideoCaptureImpl::SetGpuFactoriesHandleOnIOTaskRunner(
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (gpu_factories != gpu_factories_) {
    LOG(ERROR) << "GPU factories handle changed; assuming GPU context lost";
    gpu_factories_ = gpu_factories;
  }
}

VideoCaptureImpl::~VideoCaptureImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if ((state_ == VIDEO_CAPTURE_STATE_STARTING ||
       state_ == VIDEO_CAPTURE_STATE_STARTED) &&
      GetVideoCaptureHost())
    GetVideoCaptureHost()->Stop(device_id_);
}

void VideoCaptureImpl::SuspendCapture(bool suspend) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (suspend)
    GetVideoCaptureHost()->Pause(device_id_);
  else
    GetVideoCaptureHost()->Resume(device_id_, session_id_, params_);
}

void VideoCaptureImpl::StartCapture(
    int client_id,
    const media::VideoCaptureParams& params,
    const VideoCaptureStateUpdateCB& state_update_cb,
    const VideoCaptureDeliverFrameCB& deliver_frame_cb) {
  DVLOG(1) << __func__ << " |device_id_| = " << device_id_;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  OnLog("VideoCaptureImpl got request to start capture.");

  ClientInfo client_info;
  client_info.params = params;
  client_info.state_update_cb = state_update_cb;
  client_info.deliver_frame_cb = deliver_frame_cb;

  switch (state_) {
    case VIDEO_CAPTURE_STATE_STARTING:
    case VIDEO_CAPTURE_STATE_STARTED:
      clients_[client_id] = client_info;
      OnLog("VideoCaptureImpl capture is already started or starting.");
      // TODO(sheu): Allowing resolution change will require that all
      // outstanding clients of a capture session support resolution change.
      DCHECK_EQ(params_.resolution_change_policy,
                params.resolution_change_policy);
      return;
    case VIDEO_CAPTURE_STATE_STOPPING:
      clients_pending_on_restart_[client_id] = client_info;
      DVLOG(1) << __func__ << " Got new resolution while stopping: "
               << params.requested_format.frame_size.ToString();
      return;
    case VIDEO_CAPTURE_STATE_STOPPED:
    case VIDEO_CAPTURE_STATE_ENDED:
      clients_[client_id] = client_info;
      params_ = params;
      params_.requested_format.frame_rate =
          std::min(params_.requested_format.frame_rate,
                   static_cast<float>(media::limits::kMaxFramesPerSecond));

      DVLOG(1) << "StartCapture: starting with first resolution "
               << params_.requested_format.frame_size.ToString();
      OnLog("VideoCaptureImpl starting capture.");
      StartCaptureInternal();
      return;
    case VIDEO_CAPTURE_STATE_ERROR:
      OnLog("VideoCaptureImpl is in error state.");
      state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_ERROR);
      return;
    case VIDEO_CAPTURE_STATE_PAUSED:
    case VIDEO_CAPTURE_STATE_RESUMED:
      // The internal |state_| is never set to PAUSED/RESUMED since
      // VideoCaptureImpl is not modified by those.
      NOTREACHED();
      return;
  }
}

void VideoCaptureImpl::StopCapture(int client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  // A client ID can be in only one client list.
  // If this ID is in any client list, we can just remove it from
  // that client list and don't have to run the other following RemoveClient().
  if (!RemoveClient(client_id, &clients_pending_on_restart_)) {
    RemoveClient(client_id, &clients_);
  }

  if (!clients_.empty())
    return;
  DVLOG(1) << "StopCapture: No more client, stopping ...";
  StopDevice();
  client_buffers_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void VideoCaptureImpl::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  GetVideoCaptureHost()->RequestRefreshFrame(device_id_);
}

void VideoCaptureImpl::GetDeviceSupportedFormats(
    VideoCaptureDeviceFormatsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  GetVideoCaptureHost()->GetDeviceSupportedFormats(
      device_id_, session_id_,
      base::BindOnce(&VideoCaptureImpl::OnDeviceSupportedFormats,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void VideoCaptureImpl::GetDeviceFormatsInUse(
    VideoCaptureDeviceFormatsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  GetVideoCaptureHost()->GetDeviceFormatsInUse(
      device_id_, session_id_,
      base::BindOnce(&VideoCaptureImpl::OnDeviceFormatsInUse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void VideoCaptureImpl::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  GetVideoCaptureHost()->OnFrameDropped(device_id_, reason);
}

void VideoCaptureImpl::OnLog(const String& message) {
  GetVideoCaptureHost()->OnLog(device_id_, message);
}

void VideoCaptureImpl::SetGpuMemoryBufferSupportForTesting(
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support) {
  gpu_memory_buffer_support_ = std::move(gpu_memory_buffer_support);
}

void VideoCaptureImpl::OnStateChanged(media::mojom::VideoCaptureState state) {
  DVLOG(1) << __func__ << " state: " << state;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  // Stop the startup deadline timer as something has happened.
  startup_timeout_.Stop();

  switch (state) {
    case media::mojom::VideoCaptureState::STARTED:
      OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_STARTED");
      state_ = VIDEO_CAPTURE_STATE_STARTED;
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_STARTED);
      // In case there is any frame dropped before STARTED, always request for
      // a frame refresh to start the video call with.
      // Capture device will make a decision if it should refresh a frame.
      RequestRefreshFrame();
      break;
    case media::mojom::VideoCaptureState::STOPPED:
      OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_STOPPED");
      state_ = VIDEO_CAPTURE_STATE_STOPPED;
      client_buffers_.clear();
      weak_factory_.InvalidateWeakPtrs();
      if (!clients_.empty() || !clients_pending_on_restart_.empty()) {
        OnLog("VideoCaptureImpl restarting capture");
        RestartCapture();
      }
      break;
    case media::mojom::VideoCaptureState::PAUSED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_PAUSED);
      break;
    case media::mojom::VideoCaptureState::RESUMED:
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_RESUMED);
      break;
    case media::mojom::VideoCaptureState::FAILED:
      OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_ERROR");
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_ERROR);
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ERROR;
      break;
    case media::mojom::VideoCaptureState::ENDED:
      OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_ENDED");
      // We'll only notify the client that the stream has stopped.
      for (const auto& client : clients_)
        client.second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_STOPPED);
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ENDED;
      break;
  }
}

void VideoCaptureImpl::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::blink::VideoBufferHandlePtr buffer_handle) {
  DVLOG(1) << __func__ << " buffer_id: " << buffer_id;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  const bool inserted =
      client_buffers_
          .emplace(buffer_id, new BufferContext(std::move(buffer_handle),
                                                media_task_runner_))
          .second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferReady(
    int32_t buffer_id,
    media::mojom::blink::VideoFrameInfoPtr info) {
  DVLOG(1) << __func__ << " buffer_id: " << buffer_id;
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  bool consume_buffer = state_ == VIDEO_CAPTURE_STATE_STARTED;
  if (!consume_buffer) {
    OnFrameDropped(
        media::VideoCaptureFrameDropReason::kVideoCaptureImplNotInStartedState);
    GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer_id,
                                         media::VideoFrameFeedback());
    return;
  }

  base::TimeTicks reference_time = *info->metadata.reference_time;

  if (first_frame_ref_time_.is_null()) {
    first_frame_ref_time_ = reference_time;
    if (num_first_frame_logs_ < kMaxFirstFrameLogs) {
      OnLog("First frame received for this VideoCaptureImpl instance");
      num_first_frame_logs_++;
    } else if (num_first_frame_logs_ == kMaxFirstFrameLogs) {
      OnLog(
          "First frame received for this VideoCaptureImpl instance. This will "
          "not be logged anymore for this VideoCaptureImpl instance.");
      num_first_frame_logs_++;
    }
  }

  // If the timestamp is not prepared, we use reference time to make a rough
  // estimate. e.g. ThreadSafeCaptureOracle::DidCaptureFrame().
  // TODO(miu): Fix upstream capturers to always set timestamp and reference
  // time. See http://crbug/618407/ for tracking.
  if (info->timestamp.is_zero())
    info->timestamp = reference_time - first_frame_ref_time_;

  // TODO(qiangchen): Change the metric name to "reference_time" and
  // "timestamp", so that we have consistent naming everywhere.
  // Used by chrome/browser/media/cast_mirroring_performance_browsertest.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "OnBufferReceived",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       (reference_time - base::TimeTicks()).InMicroseconds(),
                       "time_delta", info->timestamp.InMicroseconds());

  const auto& iter = client_buffers_.find(buffer_id);
  DCHECK(iter != client_buffers_.end());
  scoped_refptr<BufferContext> buffer_context = iter->second;
  scoped_refptr<media::VideoFrame> frame;
  switch (buffer_context->buffer_type()) {
    case VideoFrameBufferHandleType::SHARED_BUFFER_HANDLE:
      // The frame is backed by a writable (unsafe) shared memory handle, but as
      // it is not sent cross-process the region does not need to be attached to
      // the frame. See also the case for READ_ONLY_SHMEM_REGION.
      if (info->strides) {
        CHECK(IsYuvPlanar(info->pixel_format) &&
              (media::VideoFrame::NumPlanes(info->pixel_format) == 3))
            << "Currently, only YUV formats support custom strides.";
        uint8_t* y_data = const_cast<uint8_t*>(buffer_context->data());
        uint8_t* u_data =
            y_data + (media::VideoFrame::Rows(media::VideoFrame::kYPlane,
                                              info->pixel_format,
                                              info->coded_size.height()) *
                      info->strides->stride_by_plane[0]);
        uint8_t* v_data =
            u_data + (media::VideoFrame::Rows(media::VideoFrame::kUPlane,
                                              info->pixel_format,
                                              info->coded_size.height()) *
                      info->strides->stride_by_plane[1]);
        frame = media::VideoFrame::WrapExternalYuvData(
            info->pixel_format, gfx::Size(info->coded_size),
            gfx::Rect(info->visible_rect), info->visible_rect.size(),
            info->strides->stride_by_plane[0],
            info->strides->stride_by_plane[1],
            info->strides->stride_by_plane[2], y_data, u_data, v_data,
            info->timestamp);
      } else {
        frame = media::VideoFrame::WrapExternalData(
            info->pixel_format, gfx::Size(info->coded_size),
            gfx::Rect(info->visible_rect), info->visible_rect.size(),
            const_cast<uint8_t*>(buffer_context->data()),
            buffer_context->data_size(), info->timestamp);
      }
      break;
    case VideoFrameBufferHandleType::READ_ONLY_SHMEM_REGION:
      // As with the SHARED_BUFFER_HANDLE type, it is sufficient to just wrap
      // the data without attaching the shared region to the frame.
      frame = media::VideoFrame::WrapExternalData(
          info->pixel_format, gfx::Size(info->coded_size),
          gfx::Rect(info->visible_rect), info->visible_rect.size(),
          const_cast<uint8_t*>(buffer_context->data()),
          buffer_context->data_size(), info->timestamp);
      break;
    case VideoFrameBufferHandleType::SHARED_MEMORY_VIA_RAW_FILE_DESCRIPTOR:
      NOTREACHED();
      break;
    case VideoFrameBufferHandleType::MAILBOX_HANDLES: {
      gpu::MailboxHolder mailbox_holder_array[media::VideoFrame::kMaxPlanes];
      CHECK_EQ(media::VideoFrame::kMaxPlanes,
               buffer_context->mailbox_holders().size());
      for (int i = 0; i < media::VideoFrame::kMaxPlanes; i++) {
        mailbox_holder_array[i] = buffer_context->mailbox_holders()[i];
      }
      frame = media::VideoFrame::WrapNativeTextures(
          info->pixel_format, mailbox_holder_array,
          media::VideoFrame::ReleaseMailboxCB(), gfx::Size(info->coded_size),
          gfx::Rect(info->visible_rect), info->visible_rect.size(),
          info->timestamp);
      break;
    }
    case VideoFrameBufferHandleType::GPU_MEMORY_BUFFER_HANDLE: {
#if defined(OS_MAC)
      // On macOS, an IOSurfaces passed as a GpuMemoryBufferHandle can be
      // used by both hardware and software paths.
      // https://crbug.com/1125879
      if (!gpu_factories_ || !media_task_runner_) {
        frame = media::VideoFrame::WrapIOSurface(
            buffer_context->TakeGpuMemoryBufferHandle(),
            gfx::Rect(info->visible_rect), info->timestamp);
        break;
      }
#endif
      CHECK(gpu_factories_);
      CHECK(media_task_runner_);
      // Create GpuMemoryBuffer from handle.
      if (!buffer_context->GetGpuMemoryBuffer()) {
        gfx::BufferFormat gfx_format;
        switch (info->pixel_format) {
          case media::VideoPixelFormat::PIXEL_FORMAT_NV12:
            gfx_format = gfx::BufferFormat::YUV_420_BIPLANAR;
            break;
          default:
            LOG(FATAL) << "Unsupported pixel format";
            return;
        }
        // The GpuMemoryBuffer is allocated and owned by the video capture
        // buffer pool from the video capture service process, so we don't need
        // to destroy the GpuMemoryBuffer here.
        auto gmb =
            gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
                buffer_context->TakeGpuMemoryBufferHandle(),
                gfx::Size(info->coded_size), gfx_format,
                gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE,
                base::DoNothing());
        buffer_context->SetGpuMemoryBuffer(std::move(gmb));
      }
      CHECK(buffer_context->GetGpuMemoryBuffer());

      // Clone the GpuMemoryBuffer and wrap it in a VideoFrame.
      std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
          gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
              buffer_context->GetGpuMemoryBuffer()->CloneHandle(),
              buffer_context->GetGpuMemoryBuffer()->GetSize(),
              buffer_context->GetGpuMemoryBuffer()->GetFormat(),
              gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE,
              base::DoNothing());

      media_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BufferContext::BindBufferToTextureOnMediaThread, gpu_factories_,
              std::move(buffer_context), std::move(info), std::move(gmb), frame,
              media::BindToCurrentLoop(base::BindOnce(
                  &VideoCaptureImpl::OnVideoFrameReady,
                  weak_factory_.GetWeakPtr(), buffer_id, reference_time)),
              media::BindToLoop(
                  main_task_runner_,
                  base::BindOnce(&VideoCaptureImpl::OnGpuContextLost,
                                 weak_factory_.GetWeakPtr()))));
      return;
    }
  }
  OnVideoFrameReady(buffer_id, reference_time, std::move(info),
                    std::move(frame), std::move(buffer_context));
}

void VideoCaptureImpl::OnVideoFrameReady(
    int32_t buffer_id,
    base::TimeTicks reference_time,
    media::mojom::blink::VideoFrameInfoPtr info,
    scoped_refptr<media::VideoFrame> frame,
    scoped_refptr<BufferContext> buffer_context) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  if (!frame) {
    OnFrameDropped(media::VideoCaptureFrameDropReason::
                       kVideoCaptureImplFailedToWrapDataAsMediaVideoFrame);
    GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer_id,
                                         media::VideoFrameFeedback());
    return;
  }

  frame->AddDestructionObserver(base::BindOnce(
      &VideoCaptureImpl::DidFinishConsumingFrame, frame->feedback(),
      media::BindToCurrentLoop(base::BindOnce(
          &VideoCaptureImpl::OnAllClientsFinishedConsumingFrame,
          weak_factory_.GetWeakPtr(), buffer_id, std::move(buffer_context)))));

  if (info->color_space.has_value() && info->color_space->IsValid())
    frame->set_color_space(info->color_space.value());

  media::VideoFrameMetadata metadata = info->metadata;
  frame->metadata()->MergeMetadataFrom(&metadata);

  // TODO(qiangchen): Dive into the full code path to let frame metadata hold
  // reference time rather than using an extra parameter.
  for (const auto& client : clients_)
    client.second.deliver_frame_cb.Run(frame, reference_time);
}

void VideoCaptureImpl::OnBufferDestroyed(int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  const auto& cb_iter = client_buffers_.find(buffer_id);
  if (cb_iter != client_buffers_.end()) {
    DCHECK(!cb_iter->second.get() || cb_iter->second->HasOneRef())
        << "Instructed to delete buffer we are still using.";
    client_buffers_.erase(cb_iter);
  }
}

constexpr base::TimeDelta VideoCaptureImpl::kCaptureStartTimeout;

void VideoCaptureImpl::OnAllClientsFinishedConsumingFrame(
    int buffer_id,
    scoped_refptr<BufferContext> buffer_context,
    const media::VideoFrameFeedback feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

// Subtle race note: It's important that the |buffer_context| argument be
// std::move()'ed to this method and never copied. This is so that the caller,
// DidFinishConsumingFrame(), does not implicitly retain a reference while it
// is running the trampoline callback on another thread. This is necessary to
// ensure the reference count on the BufferContext will be correct at the time
// OnBufferDestroyed() is called. http://crbug.com/797851
#if DCHECK_IS_ON()
  // The BufferContext should have exactly two references to it at this point,
  // one is this method's second argument and the other is from
  // |client_buffers_|.
  DCHECK(!buffer_context->HasOneRef());
  BufferContext* const buffer_raw_ptr = buffer_context.get();
  buffer_context = nullptr;
  // Now there should be only one reference, from |client_buffers_|.
  // TODO(https://crbug.com/1128853): This DCHECK is invalid for GpuMemoryBuffer
  // backed frames, because MailboxHolderReleased may hold on to a reference to
  // |buffer_context|.
  if (buffer_raw_ptr->buffer_type() !=
      VideoFrameBufferHandleType::GPU_MEMORY_BUFFER_HANDLE) {
    DCHECK(buffer_raw_ptr->HasOneRef());
  }
#else
  buffer_context = nullptr;
#endif

  GetVideoCaptureHost()->ReleaseBuffer(device_id_, buffer_id, feedback);
}

void VideoCaptureImpl::StopDevice() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (state_ != VIDEO_CAPTURE_STATE_STARTING &&
      state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;
  state_ = VIDEO_CAPTURE_STATE_STOPPING;
  OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_STOPPING");
  GetVideoCaptureHost()->Stop(device_id_);
  params_.requested_format.frame_size.SetSize(0, 0);
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPED);

  int width = 0;
  int height = 0;
  clients_.insert(clients_pending_on_restart_.begin(),
                  clients_pending_on_restart_.end());
  clients_pending_on_restart_.clear();
  for (const auto& client : clients_) {
    width = std::max(width,
                     client.second.params.requested_format.frame_size.width());
    height = std::max(
        height, client.second.params.requested_format.frame_size.height());
  }
  params_.requested_format.frame_size.SetSize(width, height);
  DVLOG(1) << __func__ << " " << params_.requested_format.frame_size.ToString();
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  state_ = VIDEO_CAPTURE_STATE_STARTING;
  OnLog("VideoCaptureImpl changing state to VIDEO_CAPTURE_STATE_STARTING");

  if (base::FeatureList::IsEnabled(kTimeoutHangingVideoCaptureStarts)) {
    startup_timeout_.Start(FROM_HERE, kCaptureStartTimeout,
                           base::BindOnce(&VideoCaptureImpl::OnStartTimedout,
                                          base::Unretained(this)));
  }

  GetVideoCaptureHost()->Start(device_id_, session_id_, params_,
                               observer_receiver_.BindNewPipeAndPassRemote());
}

void VideoCaptureImpl::OnStartTimedout() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  OnLog("VideoCaptureImpl timed out during starting");
  OnStateChanged(media::mojom::VideoCaptureState::FAILED);
}

void VideoCaptureImpl::OnDeviceSupportedFormats(
    VideoCaptureDeviceFormatsCallback callback,
    const Vector<media::VideoCaptureFormat>& supported_formats) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  std::move(callback).Run(supported_formats);
}

void VideoCaptureImpl::OnDeviceFormatsInUse(
    VideoCaptureDeviceFormatsCallback callback,
    const Vector<media::VideoCaptureFormat>& formats_in_use) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  std::move(callback).Run(formats_in_use);
}

bool VideoCaptureImpl::RemoveClient(int client_id, ClientInfoMap* clients) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  const ClientInfoMap::iterator it = clients->find(client_id);
  if (it == clients->end())
    return false;

  it->second.state_update_cb.Run(blink::VIDEO_CAPTURE_STATE_STOPPED);
  clients->erase(it);
  return true;
}

media::mojom::blink::VideoCaptureHost* VideoCaptureImpl::GetVideoCaptureHost() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  if (video_capture_host_for_testing_)
    return video_capture_host_for_testing_;

  if (!video_capture_host_.is_bound())
    video_capture_host_.Bind(std::move(pending_video_capture_host_));
  return video_capture_host_.get();
}

// static
void VideoCaptureImpl::DidFinishConsumingFrame(
    const media::VideoFrameFeedback* feedback,
    BufferFinishedCallback callback_to_io_thread) {
  // Note: This function may be called on any thread by the VideoFrame
  // destructor.  |metadata| is still valid for read-access at this point.
  std::move(callback_to_io_thread).Run(*feedback);
}

}  // namespace blink
