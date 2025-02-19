// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/video_effects_processor_webgpu.h"

#include <cstdint>
#include <memory>
#include <numbers>
#include <string_view>

#include "base/bit_cast.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "components/viz/common/resources/shared_image_format.h"
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
#include "services/video_effects/calculators/video_effects_graph_config.h"
#include "services/video_effects/calculators/video_effects_graph_webgpu.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-shared.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/dawn/include/dawn/webgpu_cpp_print.h"
#include "third_party/mediapipe/buildflags.h"

namespace {

// Creates a shared image using `frame_info.coded_size`.
[[maybe_unused]] scoped_refptr<gpu::ClientSharedImage> CreateSharedImageRGBA(
    gpu::SharedImageInterface* sii,
    const media::mojom::VideoFrameInfo& frame_info,
    gpu::SharedImageUsageSet gpu_usage,
    std::string_view debug_label) {
  scoped_refptr<gpu::ClientSharedImage> destination = sii->CreateSharedImage(
      {viz::SinglePlaneFormat::kRGBA_8888, frame_info.coded_size,
       frame_info.color_space, gpu_usage, debug_label},
      gpu::kNullSurfaceHandle);
  CHECK(destination);
  CHECK(!destination->mailbox().IsZero());

  return destination;
}

[[maybe_unused]] wgpu::Texture CreateFloat32Texture(
    const wgpu::Device& device,
    uint32_t width,
    uint32_t height,
    wgpu::TextureUsage usage,
    base::cstring_view label = {}) {
  wgpu::TextureDescriptor texture_descriptor = {
      .label = label.c_str(),
      .usage = usage,
      .dimension = wgpu::TextureDimension::e2D,
      .size = {.width = width, .height = height, .depthOrArrayLayers = 1},
      .format = wgpu::TextureFormat::RGBA32Float};
  return device.CreateTexture(&texture_descriptor);
}

enum class Conversion {
  kFromFloats,
  kToFloats,
};

[[maybe_unused]] wgpu::CommandBuffer GetTextureConversionCommandBuffer(
    const wgpu::Device& device,
    const wgpu::Texture& source,
    const wgpu::Texture& destination,
    const wgpu::RenderPipeline& render_pipeline,
    Conversion conversion,
    const wgpu::Buffer& uniforms_buffer,
    bool perform_y_flip,
    base::cstring_view label_prefix = {}) {
  const std::string command_encoder_label =
      base::StringPrintf("%s%s", label_prefix, "CommandEncoder");
  wgpu::CommandEncoderDescriptor command_encoder_descriptor = {
      .label = command_encoder_label.c_str()};
  wgpu::CommandEncoder command_encoder =
      device.CreateCommandEncoder(&command_encoder_descriptor);

  wgpu::RenderPassColorAttachment destination_color_attachment = {
      .view = destination.CreateView(),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      // Set the clear color for the destination texture with the color
      // depending on the conversion direction. This helps with debugging - in
      // case there is an underdraw, the pixels that weren't written to will
      // still have the clear color.
      .clearValue =
          {
              .r = conversion == Conversion::kToFloats ? 1.0 : 0.0,
              .g = 0.0,
              .b = conversion == Conversion::kFromFloats ? 1.0 : 0.0,
              .a = 1.0,
          },
  };

  const std::vector<wgpu::BindGroupEntry> entries = {
      {.binding = 0, .sampler = device.CreateSampler()},
      {.binding = 1, .textureView = source.CreateView()},
      {.binding = 2, .buffer = uniforms_buffer},
  };

  const std::string bind_group_label =
      base::StringPrintf("%s%s", label_prefix, "BindGroup");
  wgpu::BindGroupDescriptor bind_group_descriptor = {
      .label = bind_group_label.c_str(),
      .layout = render_pipeline.GetBindGroupLayout(0),
      .entryCount = entries.size(),
      .entries = entries.data(),
  };

  const uint32_t uniforms_perform_y_flip = perform_y_flip;
  auto uniforms_bytes = base::bit_cast<
      std::array<const uint8_t, sizeof(uniforms_perform_y_flip)>>(
      uniforms_perform_y_flip);
  command_encoder.WriteBuffer(uniforms_buffer, 0, uniforms_bytes.data(),
                              uniforms_bytes.size());

  const std::string render_pass_label =
      base::StringPrintf("%s%s", label_prefix, "RenderPass");
  wgpu::RenderPassDescriptor render_pass_descriptor = {
      .label = render_pass_label.c_str(),
      .colorAttachmentCount = 1,
      .colorAttachments = &destination_color_attachment,
  };
  wgpu::RenderPassEncoder render_pass_encoder =
      command_encoder.BeginRenderPass(&render_pass_descriptor);
  render_pass_encoder.SetPipeline(render_pipeline);
  render_pass_encoder.SetBindGroup(
      0, device.CreateBindGroup(&bind_group_descriptor));
  // We're rendering a quad which we built from 2 triangles (hard-coded in the
  // shader).
  render_pass_encoder.Draw(6);
  render_pass_encoder.End();

  const std::string command_buffer_label =
      base::StringPrintf("%s%s", label_prefix, "CommandBuffer");
  wgpu::CommandBufferDescriptor command_buffer_descriptor = {
      .label = command_buffer_label.c_str()};
  return command_encoder.Finish(&command_buffer_descriptor);
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
    wgpu::Device device,
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    scoped_refptr<viz::RasterContextProvider> raster_interface_context_provider,
    scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface)
    : device_(device),
      context_provider_(std::move(context_provider)),
      raster_interface_context_provider_(
          std::move(raster_interface_context_provider)),
      shared_image_interface_(std::move(shared_image_interface)) {
  CHECK(device_);
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
  compute_pipeline_ = CreateComputePipeline();
  render_pipeline_texture_copy_into_rgbaf32_ =
      CreateRenderPipelineForTextureCopy(wgpu::TextureFormat::RGBA32Float);
  render_pipeline_texture_copy_into_rgba8unorm_ =
      CreateRenderPipelineForTextureCopy(wgpu::TextureFormat::RGBA8Unorm);

  EnsureFlush();
  return true;
}

void VideoEffectsProcessorWebGpu::SetBackgroundSegmentationModel(
    base::span<const uint8_t> model_blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  background_segmentation_model_.resize(model_blob.size());
  base::span(background_segmentation_model_).copy_from(model_blob);

  MaybeInitializeInferenceEngine();
}

void VideoEffectsProcessorWebGpu::OnFrameProcessed(wgpu::Texture texture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Currently no-op. This is because we wait until our graph is idle when
  // feeding packets into it, and if the graph is idle, we know that all WebGPU
  // commands were scheduled. We wait for them to be executed by generating sync
  // tokens and calling `gpu::GpuSupport::SignalQuery()`.
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
    const RuntimeConfig& runtime_config,
    media::mojom::VideoBufferHandlePtr input_frame_data,
    media::mojom::VideoFrameInfoPtr input_frame_info,
    media::mojom::VideoBufferHandlePtr result_frame_data,
    media::VideoPixelFormat result_pixel_format,
    mojom::VideoEffectsProcessor::PostProcessCallback post_process_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(MEDIAPIPE_BUILD_WITH_GPU_SUPPORT)
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

  if (!graph_) {
    std::move(post_process_cb)
        .Run(mojom::PostProcessResult::NewError(
            mojom::PostProcessError::kNotReady));
    return;
  }

  const uint64_t trace_id = base::trace_event::GetNextGlobalTraceId();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
      "VideoEffectsProcessorWebGpu::PostProcess", trace_id);

  // Note: this CHECK is not strictly required, but the code below assumes it
  // holds. In order to relax this requirement, we'd need to audit the code
  // below for usages of `coded_size` and `visible_rect` on `input_frame_info`.
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
          std::move(input_frame_data->get_shared_image_handle()->shared_image));
  CHECK(in_plane);
  // s3=CreateSI()
  auto in_image =
      CreateSharedImageRGBA(shared_image_interface_.get(), *input_frame_info,
                            gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
                                gpu::SHARED_IMAGE_USAGE_RASTER_WRITE,
                            "VideoEffectsProcessorInImage");
  // t3=GenSyncToken()
  // Waiting on this sync token should ensure that the `in_image` shared image
  // is ready to be used.
  auto in_image_token = shared_image_interface_->GenUnverifiedSyncToken();

  // WaitSyncToken(t2)
  shared_image_interface_->WaitSyncToken(
      result_frame_data->get_shared_image_handle()->sync_token);
  // ImportSI(s2)
  auto out_plane = shared_image_interface_->ImportSharedImage(
      std::move(result_frame_data->get_shared_image_handle()->shared_image));
  CHECK(out_plane);
  // s4=CreateSI()
  auto out_image =
      CreateSharedImageRGBA(shared_image_interface_.get(), *input_frame_info,
                            gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                                gpu::SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE,
                            "VideoEffectsProcessorOutImage");
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
  const int input_frame_width = input_frame_info->visible_rect.width();
  const int input_frame_height = input_frame_info->visible_rect.height();
  raster_interface->CopySharedImage(in_plane->mailbox(), in_image->mailbox(), 0,
                                    0, input_frame_info->visible_rect.x(),
                                    input_frame_info->visible_rect.y(),
                                    input_frame_width, input_frame_height);

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

  // Now we can import the converted input image into WebGPU.
  // w1=ImportImage(s3)

  // Note: this descriptor does not actually influence the resulting WebGPU
  // texture in any way. This just allows us to use `wgpu::Texture` reflection
  // APIs to query texture properties (i.e. calling `wgpu::Texture::GetWidth()`,
  // `wgpu::Texture::GetHeight()`, and `wgpu::Texture::GetFormat()` will now
  // work).
  CHECK_EQ(in_image->format(), viz::SinglePlaneFormat::kRGBA_8888);
  WGPUTextureDescriptor in_texture_desc = {
      .dimension = WGPUTextureDimension_2D,
      .size =
          {
              .width = static_cast<uint32_t>(input_frame_width),
              .height = static_cast<uint32_t>(input_frame_height),
              .depthOrArrayLayers = 1,
          },
      .format = WGPUTextureFormat_RGBA8Unorm,
  };
  gpu::webgpu::ReservedTexture in_reservation =
      webgpu_interface->ReserveTexture(device_.Get(), &in_texture_desc);
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

  // Note: see comment on `in_texture_desc`.
  CHECK_EQ(out_image->format(), viz::SinglePlaneFormat::kRGBA_8888);
  WGPUTextureDescriptor out_texture_desc = {
      .dimension = WGPUTextureDimension_2D,
      .size =
          {
              .width = static_cast<uint32_t>(input_frame_width),
              .height = static_cast<uint32_t>(input_frame_height),
              .depthOrArrayLayers = 1,
          },
      .format = WGPUTextureFormat_RGBA8Unorm,
  };
  gpu::webgpu::ReservedTexture out_reservation =
      webgpu_interface->ReserveTexture(device_.Get(), &out_texture_desc);
  // CopyDst because initial prototype will just do texture-to-texture copy,
  // StorageBinding because we want to also bind it as storage texture & use
  // in compute shader.
  webgpu_interface->AssociateMailbox(
      out_reservation.deviceId, out_reservation.deviceGeneration,
      out_reservation.id, out_reservation.generation,
      WGPUTextureUsage_CopyDst | WGPUTextureUsage_StorageBinding |
          WGPUTextureUsage_RenderAttachment,
      out_image->mailbox());

  // The `in_texture` and `out_texture` format is `TextureFormat::RGBA8Unorm`.
  // This is because of how the shared image format
  // (`viz::SinglePlaneFormat::kRGBA_8888`) maps to WebGPU format.
  wgpu::Texture in_texture = in_reservation.texture;
  in_texture.SetLabel("VideoEffectsProcessorInTexture");

  wgpu::Texture out_texture = out_reservation.texture;
  out_texture.SetLabel("VideoEffectsProcessorOutTexture");

  wgpu::Texture in_texture_downscaled_float32 = CreateFloat32Texture(
      device_, 256, 144,
      wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding |
          wgpu::TextureUsage::RenderAttachment,
      "VideoEffectsProcessorInTextureDownscaledFloat32");
  wgpu::CommandBuffer pre_inference_into_floats_downscale =
      GetTextureConversionCommandBuffer(
          device_, in_texture, in_texture_downscaled_float32,
          render_pipeline_texture_copy_into_rgbaf32_, Conversion::kToFloats,
          texture_copy_uniforms_buffer_, /*perform_y_flip=*/true,
          "PreInferenceIntoFloatsDownscale");
  device_.GetQueue().Submit(1, &pre_inference_into_floats_downscale);

  // w2<-RunPipeline(w1):
  // Note: this only submits the WebGPU commands to the device queue. The fact
  // that `ProcessFrame()` returned does not mean that the frame was already
  // processed - just that the appropriate commands have been scheduled to run
  // on the GPU. Same for `WaitUntilIdle()`.
  CHECK(graph_->ProcessFrame(input_frame_info->timestamp, in_texture,
                             in_texture_downscaled_float32, out_texture,
                             runtime_config));
  // We can block since all the graph does is it schedules more work on the GPU.
  // This itself should be near-instant, so the call won't block for long.
  CHECK(graph_->WaitUntilIdle());

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
                                    0, 0, input_frame_info->visible_rect.x(),
                                    input_frame_info->visible_rect.y(),
                                    input_frame_width, input_frame_height);
  raster_interface->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);

  // ScheduleCallback()
  raster_interface_context_provider_->ContextSupport()->SignalQuery(
      work_done_query,
      base::BindOnce(&VideoEffectsProcessorWebGpu::QueryDone,
                     weak_ptr_factory_.GetWeakPtr(), work_done_query, trace_id,
                     std::move(input_frame_data), std::move(input_frame_info),
                     std::move(result_frame_data), result_pixel_format,
                     std::move(post_process_cb)));
#else
  std::move(post_process_cb)
      .Run(mojom::PostProcessResult::NewError(
          mojom::PostProcessError::kUnusable));
#endif
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

void VideoEffectsProcessorWebGpu::MaybeInitializeInferenceEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(b/382718268): handle model updates.
  if (background_segmentation_model_.empty() || !device_) {
    return;
  }

#if BUILDFLAG(MEDIAPIPE_BUILD_WITH_GPU_SUPPORT)
  if (!graph_) {
    graph_ = VideoEffectsGraphWebGpu::Create();
    CHECK(graph_);

    CHECK(graph_->Start(
        StaticConfig{
            background_segmentation_model_,
        },
        base::BindRepeating(&VideoEffectsProcessorWebGpu::OnFrameProcessed,
                            weak_ptr_factory_.GetWeakPtr())));
  }
#endif
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

wgpu::RenderPipeline
VideoEffectsProcessorWebGpu::CreateRenderPipelineForTextureCopy(
    wgpu::TextureFormat destination_format) {
  if (!texture_copy_uniforms_buffer_) {
    wgpu::BufferDescriptor uniforms_descriptor = {
        .label = "VideoEffectsProcessorTextureCopyUniforms",
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(uint32_t),
        .mappedAtCreation = false,
    };
    texture_copy_uniforms_buffer_ = device_.CreateBuffer(&uniforms_descriptor);
  }

  std::vector<wgpu::BindGroupLayoutEntry> entries;
  entries.push_back({
      .binding = 0,
      .visibility = wgpu::ShaderStage::Fragment,
      .sampler =
          {
              .type = wgpu::SamplerBindingType::NonFiltering,
          },
  });

  entries.push_back(
      {.binding = 1,
       .visibility = wgpu::ShaderStage::Fragment,
       .texture = {
           .sampleType = wgpu::TextureSampleType::UnfilterableFloat,
           .viewDimension = wgpu::TextureViewDimension::e2D,
           .multisampled = false,
       }});

  entries.push_back({
      .binding = 2,
      .visibility = wgpu::ShaderStage::Fragment,
      .buffer =
          {
              .type = wgpu::BufferBindingType::Uniform,
          },
  });

  const std::string bind_group_layout_label =
      base::StrCat({"VideoEffectsProcessorTextureCopyBindGroupLayout::",
                    base::ToString(destination_format)});
  wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor = {
      .label = bind_group_layout_label.c_str(),
      .entryCount = entries.size(),
      .entries = entries.data(),
  };
  wgpu::BindGroupLayout bind_group_layout =
      device_.CreateBindGroupLayout(&bind_group_layout_descriptor);

  const std::string pipeline_layout_label =
      base::StrCat({"VideoEffectsProcessorTextureCopyPipelineLayout::",
                    base::ToString(destination_format)});
  wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor = {
      .label = pipeline_layout_label.c_str(),
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &bind_group_layout,
  };

  wgpu::ShaderModuleWGSLDescriptor shader_module_wgsl_descriptor;
  shader_module_wgsl_descriptor.code = R"(

struct Uniforms {
    perform_y_flip: u32,    // bools are not host-shareable
}

@group(0) @binding(0) var sourceTextureSampler: sampler;
@group(0) @binding(1) var sourceTexture: texture_2d<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

@vertex
fn vertex_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOutput {
  // x,y coordinates of our triangles, in clip space:
  var triangle_points = array<vec2<f32>, 6>(
    // 1st triangle:
    vec2<f32>(-1.0, -1.0), // lower left
    vec2<f32>( 1.0,  1.0), // upper right
    vec2<f32>(-1.0,  1.0), // upper left
    // 2nd triangle:
    vec2<f32>(-1.0, -1.0), // lower left
    vec2<f32>( 1.0, -1.0), // lower right
    vec2<f32>( 1.0,  1.0), // upper right
  );

  let xy = triangle_points[in_vertex_index];

  var out: VertexOutput;
  // `position` is a vec4 so let's extend x,y with z=0.0 (at near plane), w=1.0.
  // (with w=1.0, x,y,z will be unchanged during clip space -> NDC conversion)
  out.position = vec4f(xy, 0.0, 1.0);
  // Transform from clip space to UV:
  out.uv = xy.xy * 0.5 + 0.5;   // [-1; 1] => [0; 1]
  return out;
}

@fragment
fn fragment_main(in: VertexOutput) -> @location(0) vec4f {
  let uvs = select(
    in.uv, vec2f(in.uv.x, 1.0 - in.uv.y), uniforms.perform_y_flip == 1);
  return textureSample(sourceTexture, sourceTextureSampler, uvs);
}
  )";

  const std::string shader_label =
      base::StrCat({"VideoEffectsProcessorTextureCopyShader::",
                    base::ToString(destination_format)});
  wgpu::ShaderModuleDescriptor shader_module_descriptor = {
      .nextInChain = &shader_module_wgsl_descriptor,
      .label = shader_label.c_str(),
  };
  wgpu::ShaderModule shader_module =
      device_.CreateShaderModule(&shader_module_descriptor);

  wgpu::ColorTargetState color_target_state = {
      .format = destination_format,
      .blend = nullptr,
      .writeMask = wgpu::ColorWriteMask::All,
  };
  wgpu::FragmentState fragment_state = {
      .module = shader_module,
      .entryPoint = "fragment_main",
      .constantCount = 0,
      .constants = nullptr,
      .targetCount = 1,
      .targets = &color_target_state,
  };

  const std::string pipeline_label =
      base::StrCat({"VideoEffectsProcessorTextureCopyRenderPipeline::",
                    base::ToString(destination_format)});
  wgpu::RenderPipelineDescriptor pipeline_descriptor = {
      .label = pipeline_label.c_str(),
      .layout = device_.CreatePipelineLayout(&pipeline_layout_descriptor),
      .vertex =
          {
              .module = shader_module,
              .entryPoint = "vertex_main",
              .constantCount = 0,
              .constants = nullptr,
          },
      .primitive =
          {
              .topology = wgpu::PrimitiveTopology::TriangleList,
              .stripIndexFormat = wgpu::IndexFormat::Undefined,
              .frontFace = wgpu::FrontFace::CCW,
              .cullMode = wgpu::CullMode::Back,
          },
      .depthStencil = nullptr,  // no need for depth test
      .multisample = {},        // no need to multisample
      .fragment = &fragment_state,
  };

  return device_.CreateRenderPipeline(&pipeline_descriptor);
}

void VideoEffectsProcessorWebGpu::EnsureFlush() {
  if (context_provider_->WebGPUInterface()->EnsureAwaitingFlush()) {
    context_provider_->WebGPUInterface()->FlushAwaitingCommands();
  }
}

}  // namespace video_effects
