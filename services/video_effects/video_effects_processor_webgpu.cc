// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_processor_webgpu.h"

#include <memory>
#include <numbers>

#include "base/bit_cast.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/webgpu/callback.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-shared.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/dawn_proc_table.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/dawn/include/dawn/webgpu_cpp_print.h"
#include "third_party/dawn/include/dawn/wire/WireClient.h"

#if MEDIAPIPE_USE_WEBGPU
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_device_registration.h"
#endif

namespace {

scoped_refptr<gpu::ClientSharedImage> CreateSharedImageRGBA(
    gpu::SharedImageInterface* sii,
    const media::mojom::VideoFrameInfo& frame_info,
    gpu::SharedImageUsageSet gpu_usage) {
  scoped_refptr<gpu::ClientSharedImage> destination = sii->CreateSharedImage(
      {viz::SinglePlaneFormat::kRGBA_8888, frame_info.coded_size,
       frame_info.color_space, kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
       gpu_usage, "VideoEffectsProcessorIntermediateSharedImage"},
      gpu::kNullSurfaceHandle);
  CHECK(destination);
  CHECK(!destination->mailbox().IsZero());

  return destination;
}

}  // namespace

namespace video_effects {

// Must be kept in sync w/ the struct Uniforms in compute shader below.
// See `VideoEffectsProcessorWebGpu::CreateComputePipeline()`.
struct Uniforms {
  // Valid range: [-1; -1].
  float brightness = 0.0f;
  // Valid range: [-1; -1].
  float contrast = 0.0f;
  // Valid range: [-1; -1].
  float saturation = 0.0f;
};

VideoEffectsProcessorWebGpu::VideoEffectsProcessorWebGpu(
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    scoped_refptr<viz::RasterContextProvider> raster_interface_context_provider,
    scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface,
    base::OnceClosure on_unrecoverable_error)
    : context_provider_(std::move(context_provider)),
      raster_interface_context_provider_(
          std::move(raster_interface_context_provider)),
      shared_image_interface_(std::move(shared_image_interface)),
      on_unrecoverable_error_(std::move(on_unrecoverable_error)) {
  CHECK(context_provider_);
  CHECK(context_provider_->WebGPUInterface());
  CHECK(raster_interface_context_provider_);
  CHECK(shared_image_interface_);

  // We use `GL_COMMANDS_COMPLETED_CHROMIUM` query to get notified when the
  // frame has been post-processed.
  // TODO(bialpio): this should not be needed when we no longer need the raster
  // interface for pixel format conversions - WebGPU has this capability
  // built-in (see `wgpu::Queue::OnSubmittedWorkDone()`).
  CHECK(raster_interface_context_provider_->ContextCapabilities().sync_query);
}

VideoEffectsProcessorWebGpu::~VideoEffectsProcessorWebGpu() = default;

bool VideoEffectsProcessorWebGpu::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gpu::webgpu::WebGPUInterface* webgpu_interface =
      context_provider_->WebGPUInterface();

  scoped_refptr<gpu::webgpu::APIChannel> webgpu_api_channel =
      webgpu_interface->GetAPIChannel();

  // C++ wrapper for WebGPU requires us to install a proc table globally per
  // process or per thread. Here, we install them per-process.
  dawnProcSetProcs(&dawn::wire::client::GetProcs());

  // Required to create a device. Setting a synthetic token here means that
  // blob cache will be disabled in Dawn, since the mapping that is going to
  // be queried will return an empty string. For more details see
  // `GpuProcessHost::GetIsolationKey()`.
  webgpu_interface->SetWebGPUExecutionContextToken(
      blink::WebGPUExecutionContextToken(blink::DedicatedWorkerToken{}));

  instance_ = wgpu::Instance(webgpu_api_channel->GetWGPUInstance());

  auto* request_adapter_callback = gpu::webgpu::BindWGPUOnceCallback(
      [](base::WeakPtr<VideoEffectsProcessorWebGpu> processor,
         wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
         char const* message) {
        if (processor) {
          processor->OnRequestAdapter(status, std::move(adapter), message);
        }
      },
      weak_ptr_factory_.GetWeakPtr());
  instance_.RequestAdapter(nullptr, wgpu::CallbackMode::AllowSpontaneous,
                           request_adapter_callback->UnboundCallback(),
                           request_adapter_callback->AsUserdata());
  EnsureFlush();

  return true;
}

// `VideoEffectsProcessorWebGpu::PostProcess()` runs the simple shader on top of
// video frame provided to us from Video Capture Service. We use 3 different IPC
// interfaces to talk to GPU service, hence we need to rely on sync tokens for
// synchronizing different GPU contexts. The high-level strategy is: before
// using a resource on a given interface, wait on a sync token that was
// generated by a context on which the resource originated from. Additionally,
// the token must've been generated _after_ the said resource was created.
//
// Key:
// - SII - SharedImageInterface
// - RI - RasterInterface
// - WGPU - WebGPUInterface
// - s1 - SI created by Video Capture Service for input frame, available in
//      `input_frame_data->get_shared_image_handle()->shared_image`
// - t1 - sync token created by Video Capture Service, available in
//      `input_frame_data->get_shared_image_handle()->sync_token`
// - s2 - SI created by Video Capture Service for output frame, available in
//      `result_frame_data->get_shared_image_handle()->shared_image`
// - t2 - sync token created by Video Capture Service, available in
//      `result_frame_data->get_shared_image_handle()->sync_token`
//
// clang-format off
//
//   Video Capture Service                Video Effects Service
//
//           SII                        SII         RI          WGPU
//            |                          │           │            │
//       s1=CreateSI()                   │           │            │
//       t1=GenSyncToken()               │           │            │
//            │                          │           │            │
//       s2=CreateSI()                   │           │            │
//       t2=GenSyncToken()               │           │            │
//            │                          │           │            │
//            │                   WaitSyncToken(t1)  │            │
//            │                   ImportSI(s1)       │            │
//            │                          |           │            │
//            │                   s3=CreateSI()      │            │
//            │                   t3=GenSyncToken()  │            │
//            │                          |           │            │
//            │                   WaitSyncToken(t2)  │            │
//            │                   ImportSI(s2)       │            │
//            │                          |           │            │
//            │                   s4=CreateSI()      │            │
//            │                   t4=GenSyncToken()  │            │
//            │                          |           │            │
//            │                          |    WaitSyncToken(t1)   │
//            │                          |    WaitSyncToken(t3)   │
//            │                          |           |            │
//            │                          |    s3<-CopySI(s1)      │
//            │                          |    t5=GenSyncToken()   │
//            │                          |           │            │
//            │                          |           │     WaitSyncToken(t3)
//            │                          │           |     WaitSyncToken(t5)
//            │                          │           │     w1=ImportImage(s3)
//            |                          |           |            |
//            |                          |           |     WaitSyncToken(t4)
//            |                          |           |     w2=ImportImage(s4)
//            |                          |           |            |
//            |                          |           |     w2<-RunPipeline(w1)
//            |                          |           |     t6=GenSyncToken()
//            |                          |           |            |
//            |                          |    WaitSyncToken(t2)   |
//            |                          |    WaitSyncToken(t4)   |
//            |                          |    WaitSyncToken(t6)   |
//            |                          |           |            |
//            |                          |    s2<-CopySI(s4)      |
//            |                          |           |            |
//            |                          |    ScheduleCallback()  |
//            |                          |           |            |
//
// clang-format on
//
// The method body is annotated with the comments taken from the diagram above
// to make it more clear which part of the code corresponds to which step from
// the diagram.
void VideoEffectsProcessorWebGpu::PostProcess(
    media::mojom::VideoBufferHandlePtr input_frame_data,
    media::mojom::VideoFrameInfoPtr input_frame_info,
    media::mojom::VideoBufferHandlePtr result_frame_data,
    media::VideoPixelFormat result_pixel_format,
    mojom::VideoEffectsProcessor::PostProcessCallback post_process_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    std::move(post_process_cb)
        .Run(mojom::PostProcessResult::NewError(
            mojom::PostProcessError::kNotReady));
    return;
  }

  // SharedImageInterface, RasterInterface, and WebGPUInterface are all backed
  // by the same GpuChannelHost, which means they can all use unverified sync
  // tokens to synchronize the work.

  if (input_frame_info->pixel_format !=
          media::VideoPixelFormat::PIXEL_FORMAT_NV12 ||
      input_frame_info->pixel_format != result_pixel_format) {
    // Pixel formats other than NV12 are not supported yet.
    // Pixel format conversions are not supported yet.
    std::move(post_process_cb)
        .Run(mojom::PostProcessResult::NewError(
            mojom::PostProcessError::kUnknown));
    return;
  }

  if (!input_frame_data->is_shared_image_handle() ||
      !result_frame_data->is_shared_image_handle()) {
    // Operating on in-system-memory video frames is not supported yet.
    // TODO(https://crbug.com/347706984): Support in-system-memory video frames.
    // Those can legitimately show up e.g. on Windows when MediaFoundation
    // capturer is disabled, or on Linux.
    std::move(post_process_cb)
        .Run(mojom::PostProcessResult::NewError(
            mojom::PostProcessError::kUnknown));
    return;
  }

  const uint64_t trace_id = base::trace_event::GetNextGlobalTraceId();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
      "VideoEffectsProcessorWebGpu::PostProcess", trace_id);

  CHECK_EQ(input_frame_info->coded_size, input_frame_info->visible_rect.size());

  gpu::raster::RasterInterface* raster_interface =
      raster_interface_context_provider_->RasterInterface();
  CHECK(raster_interface);

  gpu::webgpu::WebGPUInterface* webgpu_interface =
      context_provider_->WebGPUInterface();
  CHECK(webgpu_interface);

  // WaitSyncToken(t1)
  // Wait for sync token that was generated after creating the shared images
  // before we try to import them.
  shared_image_interface_->WaitSyncToken(
      input_frame_data->get_shared_image_handle()->sync_token);
  // ImportSI(s1)
  scoped_refptr<gpu::ClientSharedImage> in_plane =
      shared_image_interface_->ImportSharedImage(
          input_frame_data->get_shared_image_handle()->shared_image);
  CHECK(in_plane);
  // s3=CreateSI()
  auto in_image =
      CreateSharedImageRGBA(shared_image_interface_.get(), *input_frame_info,
                            gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
                                gpu::SHARED_IMAGE_USAGE_RASTER_WRITE);
  // t3=GenSyncToken()
  // Waiting on this sync token should ensure that the `in_image` shared image
  // is ready to be used.
  auto in_image_token = shared_image_interface_->GenUnverifiedSyncToken();

  // WaitSyncToken(t2)
  shared_image_interface_->WaitSyncToken(
      result_frame_data->get_shared_image_handle()->sync_token);
  // ImportSI(s2)
  auto out_plane = shared_image_interface_->ImportSharedImage(
      result_frame_data->get_shared_image_handle()->shared_image);
  CHECK(out_plane);
  // s4=CreateSI()
  auto out_image =
      CreateSharedImageRGBA(shared_image_interface_.get(), *input_frame_info,
                            gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                gpu::SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE);
  // t4=GenSyncToken()
  // Waiting on this sync token should ensure that the `out_image` shared image
  // is ready to be used.
  auto out_image_token = shared_image_interface_->GenUnverifiedSyncToken();
  shared_image_interface_->Flush();

  // WaitSyncToken(t1)
  // Wait for SI creation sync token for source images.
  raster_interface->WaitSyncTokenCHROMIUM(
      input_frame_data->get_shared_image_handle()->sync_token.GetConstData());
  // WaitSyncToken(t3)
  // Wait for SI creation sync token for (intermediate) destination image.
  // This token was also generated after the source SIs were imported on our
  // shared image interface.
  raster_interface->WaitSyncTokenCHROMIUM(in_image_token.GetConstData());

  // At this point, our input planes and intermediate image should be usable on
  // raster interface. Proceed with pixel format conversion.

  // s3<-CopySI(s1)
  raster_interface->CopySharedImage(
      in_plane->mailbox(), in_image->mailbox(), in_image->GetTextureTarget(), 0,
      0, input_frame_info->visible_rect.x(), input_frame_info->visible_rect.y(),
      input_frame_info->visible_rect.width(),
      input_frame_info->visible_rect.height(),
      /*unpack_flip_y=*/false,
      /*unpack_premultiply_alpha=*/false);

  // Let's insert a sync token generated by raster interface after pixel
  // format conversion so that WebGPU interface could wait for it to complete.
  gpu::SyncToken post_yuv_to_rgba;
  // t5=GenSyncToken()
  // Waiting on this sync token should ensure that the effects pixel format
  // conversion commands are visible to the context on which the wait has
  // happened.
  raster_interface->GenUnverifiedSyncTokenCHROMIUM(post_yuv_to_rgba.GetData());
  raster_interface->Flush();

  // WaitSyncToken(t3)
  // Wait for SI creation sync token for (intermediate) destination image.
  // This token was also generated after the source SIs were imported on our
  // SII.
  webgpu_interface->WaitSyncTokenCHROMIUM(in_image_token.GetConstData());
  // WaitSyncToken(t5)
  // Wait for YUV to RGBA conversion operation to finish on RI before touching
  // the (intermediate) destination.
  webgpu_interface->WaitSyncTokenCHROMIUM(post_yuv_to_rgba.GetConstData());

  // w1=ImportImage(s3)
  // Now we can import the converted input image into WebGPU.
  gpu::webgpu::ReservedTexture in_reservation =
      webgpu_interface->ReserveTexture(device_.Get());
  // CopySrc because initial prototype will just do texture-to-texture copy,
  // TextureBinding because we want to also bind it as a regular texture & use
  // in compute shader.
  webgpu_interface->AssociateMailbox(
      in_reservation.deviceId, in_reservation.deviceGeneration,
      in_reservation.id, in_reservation.generation,
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_TextureBinding,
      in_image->mailbox());

  // WaitSyncToken(t4)
  // Wait for SI creation sync token for (2nd intermediate) destination image.
  // This token was also generated after the source SIs were imported on our
  // SII.
  webgpu_interface->WaitSyncTokenCHROMIUM(out_image_token.GetConstData());

  // w2=ImportImage(s4)
  gpu::webgpu::ReservedTexture out_reservation =
      webgpu_interface->ReserveTexture(device_.Get());
  // CopyDst because initial prototype will just do texture-to-texture copy,
  // StorageBinding because we want to also bind it as storage texture & use
  // in compute shader.
  webgpu_interface->AssociateMailbox(
      out_reservation.deviceId, out_reservation.deviceGeneration,
      out_reservation.id, out_reservation.generation,
      WGPUTextureUsage_CopyDst | WGPUTextureUsage_StorageBinding,
      out_image->mailbox());

  wgpu::Texture in_texture = in_reservation.texture;
  wgpu::Texture out_texture = out_reservation.texture;

  std::vector<wgpu::BindGroupEntry> entries;
  entries.push_back(wgpu::BindGroupEntry{
      .binding = 0, .textureView = in_texture.CreateView()});
  entries.push_back(wgpu::BindGroupEntry{
      .binding = 1, .textureView = out_texture.CreateView()});
  entries.push_back(
      wgpu::BindGroupEntry{.binding = 2, .buffer = uniforms_buffer_});

  wgpu::BindGroupDescriptor bind_group_descriptor = {
      .label = "VideoEffectsProcessorBindGroup",
      .layout = compute_pipeline_.GetBindGroupLayout(0),
      .entryCount = entries.size(),
      .entries = entries.data(),
  };
  wgpu::BindGroup bind_group = device_.CreateBindGroup(&bind_group_descriptor);

  // Now we need to dispatch the shader:
  wgpu::CommandEncoderDescriptor command_encoder_descriptor = {
      .label = "VideoEffectsProcessorCommandEncoder"};
  wgpu::CommandEncoder command_encoder =
      device_.CreateCommandEncoder(&command_encoder_descriptor);

  const base::TimeDelta input_frame_timestamp = input_frame_info->timestamp;
  const base::TimeDelta effects_period = base::Seconds(10);

  const auto rads = (static_cast<float>(input_frame_timestamp.InMilliseconds() %
                                        effects_period.InMilliseconds()) /
                     effects_period.InMilliseconds()) *
                    2 * std::numbers::pi;
  const float brightness = std::sin(rads);
  const float contrast = 0.0f;
  const float saturation = 0.0f;

  const Uniforms uniforms = {
      .brightness = brightness,
      .contrast = contrast,
      .saturation = saturation,
  };
  auto uniforms_bytes =
      base::bit_cast<std::array<const uint8_t, sizeof(uniforms)>>(uniforms);
  command_encoder.WriteBuffer(uniforms_buffer_, 0, uniforms_bytes.data(),
                              uniforms_bytes.size());

  wgpu::ComputePassDescriptor compute_pass_descriptor = {
      .label = "VideoEffectsProcessorComputePassEncoder"};
  wgpu::ComputePassEncoder compute_pass_encoder =
      command_encoder.BeginComputePass(&compute_pass_descriptor);

  compute_pass_encoder.SetPipeline(compute_pipeline_);
  compute_pass_encoder.SetBindGroup(0, bind_group);
  compute_pass_encoder.DispatchWorkgroups(
      base::ClampCeil(input_frame_info->coded_size.width() / 16.0f),
      base::ClampCeil(input_frame_info->coded_size.height() / 16.0f), 1);
  compute_pass_encoder.End();

  wgpu::CommandBufferDescriptor command_buffer_descriptor = {
      .label = "VideoEffectsProcessorCommandBuffer"};
  wgpu::CommandBuffer commandBuffer =
      command_encoder.Finish(&command_buffer_descriptor);

  // w2<-RunPipeline(w1)
  default_queue_.Submit(1, &commandBuffer);

  webgpu_interface->DissociateMailbox(in_reservation.id,
                                      in_reservation.generation);
  in_reservation = {};

  webgpu_interface->DissociateMailbox(out_reservation.id,
                                      out_reservation.generation);
  out_reservation = {};

  gpu::SyncToken post_copy_sync_token;
  // t6=GenSyncToken()
  webgpu_interface->GenUnverifiedSyncTokenCHROMIUM(
      post_copy_sync_token.GetData());
  EnsureFlush();

  // WaitSyncToken(t2)
  // Raster interface should wait on the sync tokens of the imported planes that
  // are meant to contain the end-result:
  raster_interface->WaitSyncTokenCHROMIUM(
      result_frame_data->get_shared_image_handle()->sync_token.GetConstData());
  // WaitSyncToken(t4)
  raster_interface->WaitSyncTokenCHROMIUM(out_image_token.GetConstData());
  // WaitSyncToken(t6)
  // We should now have the texture contents in `out_image` &
  // we need to convert them back to NV12. We'll use it on raster interface,
  // and it was produced by WebGPU so let's add a sync:
  raster_interface->WaitSyncTokenCHROMIUM(post_copy_sync_token.GetConstData());

  // Since the consumers of video frames originating in Video Capture Service do
  // not use sync tokens to synchronize access to the video frames, we cannot
  // use that mechanism here. Instead, we use `GL_COMMANDS_COMPLETED_CHROMIUM`
  // query, which will run the callback once the graphical API commands have
  // finished running on the GPU. We'll only surface the post-processed video
  // frames to our callers after this happens.
  GLuint work_done_query = {};
  raster_interface->GenQueriesEXT(1, &work_done_query);
  CHECK(work_done_query);

  raster_interface->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM,
                                  work_done_query);
  // s2<-CopySI(s4)
  raster_interface->CopySharedImage(out_image->mailbox(), out_plane->mailbox(),
                                    out_plane->GetTextureTarget(), 0, 0,
                                    input_frame_info->visible_rect.x(),
                                    input_frame_info->visible_rect.y(),
                                    input_frame_info->visible_rect.width(),
                                    input_frame_info->visible_rect.height(),
                                    /*unpack_flip_y=*/false,
                                    /*unpack_premultiply_alpha=*/false);
  raster_interface->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);

  // ScheduleCallback()
  raster_interface_context_provider_->ContextSupport()->SignalQuery(
      work_done_query,
      base::BindOnce(&VideoEffectsProcessorWebGpu::QueryDone,
                     weak_ptr_factory_.GetWeakPtr(), work_done_query, trace_id,
                     std::move(input_frame_data), std::move(input_frame_info),
                     std::move(result_frame_data), result_pixel_format,
                     std::move(post_process_cb)));
}

void VideoEffectsProcessorWebGpu::QueryDone(
    GLuint query_id,
    uint64_t trace_id,
    media::mojom::VideoBufferHandlePtr input_frame_data,
    media::mojom::VideoFrameInfoPtr input_frame_info,
    media::mojom::VideoBufferHandlePtr result_frame_data,
    media::VideoPixelFormat result_pixel_format,
    mojom::VideoEffectsProcessor::PostProcessCallback post_process_cb) {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
      "VideoEffectsProcessorWebGpu::PostProcess", trace_id);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gpu::raster::RasterInterface* raster_interface =
      raster_interface_context_provider_->RasterInterface();
  CHECK(raster_interface);

  raster_interface->DeleteQueriesEXT(1, &query_id);

  // TODO(bialpio): the result should be identical to the input, hence we can
  // pass the `input_frame_info` unmodified here. This will need to change once
  // we support pixel format conversions.
  std::move(post_process_cb)
      .Run(mojom::PostProcessResult::NewSuccess(
          mojom::PostProcessSuccess::New(std::move(input_frame_info))));
}

void VideoEffectsProcessorWebGpu::OnRequestAdapter(
    wgpu::RequestAdapterStatus status,
    wgpu::Adapter adapter,
    char const* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != wgpu::RequestAdapterStatus::Success || !adapter) {
    MaybeCallOnUnrecoverableError();
    return;
  }

  adapter_ = std::move(adapter);

  // TODO(bialpio): Determine the limits based on the incoming video frames.
  wgpu::RequiredLimits limits = {
      .limits = {},
  };

  auto* device_lost_callback = gpu::webgpu::BindWGPUOnceCallback(
      [](base::WeakPtr<VideoEffectsProcessorWebGpu> processor,
         WGPUDeviceLostReason reason, char const* message) {
        if (processor) {
          processor->OnDeviceLost(reason, message);
        }
      },
      weak_ptr_factory_.GetWeakPtr());
  wgpu::DeviceDescriptor descriptor;
  descriptor.label = "VideoEffectsProcessor";
  descriptor.requiredLimits = &limits;
  descriptor.defaultQueue = {
      .label = "VideoEffectsProcessorDefaultQueue",
  };
  descriptor.deviceLostCallback = device_lost_callback->UnboundCallback();
  descriptor.deviceLostUserdata = device_lost_callback->AsUserdata();

  auto* request_device_callback = gpu::webgpu::BindWGPUOnceCallback(
      [](base::WeakPtr<VideoEffectsProcessorWebGpu> processor,
         wgpu::RequestDeviceStatus status, wgpu::Device device,
         char const* message) {
        if (processor) {
          processor->OnRequestDevice(status, std::move(device), message);
        }
      },
      weak_ptr_factory_.GetWeakPtr());
  adapter_.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous,
                         request_device_callback->UnboundCallback(),
                         request_device_callback->AsUserdata());
  EnsureFlush();
}

void VideoEffectsProcessorWebGpu::OnRequestDevice(
    wgpu::RequestDeviceStatus status,
    wgpu::Device device,
    char const* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != wgpu::RequestDeviceStatus::Success || !device) {
    MaybeCallOnUnrecoverableError();
    return;
  }

  device_ = device;
  device_.SetUncapturedErrorCallback(&ErrorCallback, nullptr);
  device_.SetLoggingCallback(&LoggingCallback, nullptr);

#if MEDIAPIPE_USE_WEBGPU
  // TODO(b/366236619): Move device registration to VideoEffectsServiceImpl.
  mediapipe::WebGpuDeviceRegistration::GetInstance().RegisterWebGpuDevice(
      std::move(device));
#endif

  default_queue_ = device_.GetQueue();
  compute_pipeline_ = CreateComputePipeline();
  EnsureFlush();
}

wgpu::ComputePipeline VideoEffectsProcessorWebGpu::CreateComputePipeline() {
  wgpu::BufferDescriptor uniforms_descriptor = {
      .label = "VideoEffectsProcessorUniformBuffer",
      .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
      .size = sizeof(Uniforms),
      .mappedAtCreation = false,
  };

  uniforms_buffer_ = device_.CreateBuffer(&uniforms_descriptor);

  std::vector<wgpu::BindGroupLayoutEntry> bindings;
  bindings.push_back({
      .binding = 0,
      .visibility = wgpu::ShaderStage::Compute,
      .texture =
          {
              .sampleType = wgpu::TextureSampleType::Float,
              .viewDimension = wgpu::TextureViewDimension::e2D,
              .multisampled = false,
          },
  });

  bindings.push_back({
      .binding = 1,
      .visibility = wgpu::ShaderStage::Compute,
      .storageTexture =
          {
              .access = wgpu::StorageTextureAccess::WriteOnly,
              .format = wgpu::TextureFormat::RGBA8Unorm,
              .viewDimension = wgpu::TextureViewDimension::e2D,
          },
  });

  bindings.push_back({.binding = 2,
                      .visibility = wgpu::ShaderStage::Compute,
                      .buffer = {
                          .type = wgpu::BufferBindingType::Uniform,
                      }});

  wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor = {
      .label = "VideoEffectsProcessorBindGroupLayout",
      .entryCount = bindings.size(),
      .entries = bindings.data(),
  };
  wgpu::BindGroupLayout bind_group_layout =
      device_.CreateBindGroupLayout(&bind_group_layout_descriptor);

  wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor = {
      .label = "VideoEffectsProcessorPipelineLayout",
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &bind_group_layout,
  };
  wgpu::PipelineLayout pipeline_layout =
      device_.CreatePipelineLayout(&pipeline_layout_descriptor);

  wgpu::ShaderSourceWGSL compute_shader_wgsl_descriptor;
  compute_shader_wgsl_descriptor.code = R"(
struct Uniforms {
    // Valid range: [-1; -1].
    brightness: f32,
    // Valid range: [-1; -1].
    contrast: f32,
    // Valid range: [-1; -1].
    saturation: f32,
}

@group(0) @binding(0) var inputBuffer: texture_2d<f32>;
@group(0) @binding(1) var outputBuffer: texture_storage_2d<rgba8unorm, write>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

// TODO(bialpio): We may want to have multiple shader versions depending on the
// maximum supported workgroup size. WebGPU guarantees that the maximum is not
// going to be lower than 256 invocations, so let's start with that.
// Note: `workgroup_size()` MUST be updated at the same time that the call to
// `wgpu::ComputePassEncoder::DispatchWorkgroups()` that dispatches this shader
// is updated.
@compute @workgroup_size(16, 16, 1)
fn postProcess(@builtin(global_invocation_id) id: vec3<u32>) {
  let size = textureDimensions(inputBuffer);
  let position = id.xy;
  if (all(position < size)) {
    var color: vec4<f32> = textureLoad(inputBuffer, position, 0);

    color = (color - 0.5) * (uniforms.contrast + 1) + 0.5;
    color = color + uniforms.brightness;
    // TODO(bialpio): apply saturation here.

    textureStore(outputBuffer, position, color);
  }
}
    )";

  wgpu::ShaderModuleDescriptor compute_shader_descriptor = {
      .nextInChain = &compute_shader_wgsl_descriptor,
      .label = "VideoEffectsProcessorComputeShader",
  };
  wgpu::ShaderModule compute_shader =
      device_.CreateShaderModule(&compute_shader_descriptor);

  wgpu::ComputePipelineDescriptor compute_pipeline_descriptor = {
      .label = "VideoEffectsProcessorComputePipeline",
      .layout = pipeline_layout,
      .compute =
          {
              .module = compute_shader,
              .entryPoint = "postProcess",
          },
  };

  return device_.CreateComputePipeline(&compute_pipeline_descriptor);
}

void VideoEffectsProcessorWebGpu::OnDeviceLost(WGPUDeviceLostReason reason,
                                               char const* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if MEDIAPIPE_USE_WEBGPU
  mediapipe::WebGpuDeviceRegistration::GetInstance().UnRegisterWebGpuDevice();
#endif
  device_ = {};

  MaybeCallOnUnrecoverableError();
}

void VideoEffectsProcessorWebGpu::EnsureFlush() {
  if (context_provider_->WebGPUInterface()->EnsureAwaitingFlush()) {
    context_provider_->WebGPUInterface()->FlushAwaitingCommands();
  }
}

void VideoEffectsProcessorWebGpu::MaybeCallOnUnrecoverableError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (on_unrecoverable_error_) {
    std::move(on_unrecoverable_error_).Run();
  }
}

// static
void VideoEffectsProcessorWebGpu::ErrorCallback(WGPUErrorType type,
                                                char const* message,
                                                void* userdata) {
  LOG(ERROR) << "VideoEffectsProcessor encountered a WebGPU error. type: "
             << type << ", message: " << (message ? message : "(unavailable)");
}

// static
void VideoEffectsProcessorWebGpu::LoggingCallback(WGPULoggingType type,
                                                  char const* message,
                                                  void* userdata) {
  auto log_line = base::StringPrintf(
      "VideoEffectsProcessor received WebGPU log message. message: %s",
      (message ? message : "(unavailable)"));

  switch (type) {
    case WGPULoggingType_Verbose:
      [[fallthrough]];
    case WGPULoggingType_Info:
      VLOG(1) << log_line;
      break;
    case WGPULoggingType_Warning:
      LOG(WARNING) << log_line;
      break;
    case WGPULoggingType_Error:
      LOG(ERROR) << log_line;
      break;
    default:
      VLOG(1) << log_line;
      break;
  }
}

}  // namespace video_effects
