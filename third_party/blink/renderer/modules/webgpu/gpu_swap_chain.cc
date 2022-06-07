// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_swap_chain.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_canvas_context.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

GPUSwapChain::GPUSwapChain(GPUCanvasContext* context,
                           GPUDevice* device,
                           WGPUTextureUsage usage,
                           WGPUTextureFormat format,
                           cc::PaintFlags::FilterQuality filter_quality,
                           V8GPUCanvasAlphaMode::Enum alpha_mode,
                           gfx::Size size)
    : DawnObjectBase(device->GetDawnControlClient()),
      device_(device),
      context_(context),
      usage_(usage),
      format_(format),
      alpha_mode_(alpha_mode),
      size_(size) {
  // TODO: Use label from GPUObjectDescriptorBase.
  swap_buffers_ = base::AdoptRef(new WebGPUSwapBufferProvider(
      this, GetDawnControlClient(), device->GetHandle(), usage_, format));
  swap_buffers_->SetFilterQuality(filter_quality);

  // Note: SetContentsOpaque is only an optimization hint. It doesn't
  // actually make the contents opaque.
  switch (alpha_mode) {
    case V8GPUCanvasAlphaMode::Enum::kOpaque: {
      CcLayer()->SetContentsOpaque(true);

      WGPUShaderModuleWGSLDescriptor wgsl_desc = {
          .chain = {.sType = WGPUSType_ShaderModuleWGSLDescriptor},
          .source = R"(
          @vertex fn vert_main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 3.0, -1.0),
                vec2<f32>(-1.0,  3.0));
            return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
          }

          @fragment fn frag_main() -> @location(0) vec4<f32> {
            return vec4<f32>(1.0);
          }
        )",
      };
      WGPUShaderModuleDescriptor shader_module_desc = {.nextInChain =
                                                           &wgsl_desc.chain};
      WGPUShaderModule shader_module = GetProcs().deviceCreateShaderModule(
          device_->GetHandle(), &shader_module_desc);

      WGPUColorTargetState color_target = {
          .format = format_,
          .writeMask = WGPUColorWriteMask_Alpha,
      };
      WGPUFragmentState fragment = {
          .module = shader_module,
          .entryPoint = "frag_main",
          .targetCount = 1,
          .targets = &color_target,
      };
      WGPURenderPipelineDescriptor pipeline_desc = {
          .vertex =
              {
                  .module = shader_module,
                  .entryPoint = "vert_main",
              },
          .primitive = {.topology = WGPUPrimitiveTopology_TriangleList},
          .multisample = {.count = 1, .mask = 0xFFFFFFFF},
          .fragment = &fragment,
      };
      alpha_to_one_pipeline_ = GetProcs().deviceCreateRenderPipeline(
          device_->GetHandle(), &pipeline_desc);
      GetProcs().shaderModuleRelease(shader_module);
      break;
    }
    case V8GPUCanvasAlphaMode::Enum::kPremultiplied:
      CcLayer()->SetContentsOpaque(false);
      break;
  }
}

GPUSwapChain::~GPUSwapChain() {
  Neuter();
}

void GPUSwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(context_);
  visitor->Trace(texture_);
}

void GPUSwapChain::Neuter() {
  if (alpha_to_one_pipeline_ != nullptr) {
    GetProcs().renderPipelineRelease(alpha_to_one_pipeline_);
    alpha_to_one_pipeline_ = nullptr;
  }
  texture_ = nullptr;
  if (swap_buffers_) {
    swap_buffers_->Neuter();
    swap_buffers_ = nullptr;
  }
}

cc::Layer* GPUSwapChain::CcLayer() {
  DCHECK(swap_buffers_);
  return swap_buffers_->CcLayer();
}

void GPUSwapChain::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  DCHECK(swap_buffers_);
  if (swap_buffers_) {
    swap_buffers_->SetFilterQuality(filter_quality);
  }
}

scoped_refptr<StaticBitmapImage> GPUSwapChain::TransferToStaticBitmapImage() {
  viz::TransferableResource transferable_resource;
  viz::ReleaseCallback release_callback;
  if (!swap_buffers_->PrepareTransferableResource(
          nullptr, &transferable_resource, &release_callback)) {
    // If we can't get a mailbox, return an transparent black ImageBitmap.
    // The only situation in which this could happen is when two or more calls
    // to transferToImageBitmap are made back-to-back, or when the context gets
    // lost. We intentionally leave the transparent black image in legacy color
    // space.
    SkBitmap black_bitmap;
    black_bitmap.allocN32Pixels(transferable_resource.size.width(),
                                transferable_resource.size.height());
    black_bitmap.eraseARGB(0, 0, 0, 0);
    return UnacceleratedStaticBitmapImage::Create(
        SkImage::MakeFromBitmap(black_bitmap));
  }
  DCHECK(release_callback);

  // We reuse the same mailbox name from above since our texture id was consumed
  // from it.
  const auto& sk_image_mailbox = transferable_resource.mailbox_holder.mailbox;
  // Use the sync token generated after producing the mailbox. Waiting for this
  // before trying to use the mailbox with some other context will ensure it is
  // valid.
  const auto& sk_image_sync_token =
      transferable_resource.mailbox_holder.sync_token;

  auto sk_color_type = viz::ResourceFormatToClosestSkColorType(
      /*gpu_compositing=*/true, transferable_resource.format);

  const SkImageInfo sk_image_info = SkImageInfo::Make(
      size_.width(), size_.height(), sk_color_type, kPremul_SkAlphaType);

  return AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      sk_image_mailbox, sk_image_sync_token, /* shared_image_texture_id = */ 0,
      sk_image_info, transferable_resource.mailbox_holder.texture_target,
      /* is_origin_top_left = */ kBottomLeft_GrSurfaceOrigin,
      swap_buffers_->GetContextProviderWeakPtr(),
      base::PlatformThread::CurrentRef(), Thread::Current()->GetTaskRunner(),
      std::move(release_callback), /*supports_display_compositing=*/true,
      transferable_resource.is_overlay_candidate);
}

scoped_refptr<CanvasResource> GPUSwapChain::ExportCanvasResource() {
  viz::TransferableResource transferable_resource;
  viz::ReleaseCallback release_callback;
  if (!swap_buffers_->PrepareTransferableResource(
          nullptr, &transferable_resource, &release_callback)) {
    return nullptr;
  }

  SkImageInfo resource_info = SkImageInfo::Make(
      transferable_resource.size.width(), transferable_resource.size.height(),
      viz::ResourceFormatToClosestSkColorType(
          /*gpu_compositing=*/true, transferable_resource.format),
      kPremul_SkAlphaType);
  return ExternalCanvasResource::Create(
      transferable_resource.mailbox_holder.mailbox, std::move(release_callback),
      transferable_resource.mailbox_holder.sync_token, resource_info,
      transferable_resource.mailbox_holder.texture_target,
      swap_buffers_->GetContextProviderWeakPtr(), /*resource_provider=*/nullptr,
      cc::PaintFlags::FilterQuality::kLow,
      /*is_origin_top_left=*/kBottomLeft_GrSurfaceOrigin,
      transferable_resource.is_overlay_candidate);
}

scoped_refptr<StaticBitmapImage> GPUSwapChain::Snapshot() const {
  // If there is a current texture, create a snapshot from it.
  if (texture_) {
    return SnapshotInternal(texture_->GetHandle(), Size());
  }

  // If there is no current texture, we need to get the information of the last
  // texture reserved, that contains the last mailbox, create a new texture for
  // it, and use it to create the resource provider. We also need the size of
  // the texture to create the resource provider.
  auto mailbox_texture_size =
      swap_buffers_->GetLastWebGPUMailboxTextureAndSize();
  if (!mailbox_texture_size.mailbox_texture)
    return nullptr;
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      mailbox_texture_size.mailbox_texture;
  gfx::Size size = mailbox_texture_size.size;

  return SnapshotInternal(mailbox_texture->GetTexture(), size);
}

scoped_refptr<StaticBitmapImage> GPUSwapChain::SnapshotInternal(
    const WGPUTexture& texture,
    const gfx::Size& size) const {
  const auto info = SkImageInfo::Make(size.width(), size.height(),
                                      viz::ResourceFormatToClosestSkColorType(
                                          /*gpu_compositing=*/true, Format()),
                                      kPremul_SkAlphaType);
  // We tag the SharedImage inside the WebGPUImageProvider with display usage
  // since there are uncommon paths which may use this snapshot for compositing.
  // These paths are usually related to either printing or either video and
  // usually related to OffscreenCanvas; in cases where the image created from
  // this Snapshot will be sent eventually to the Display Compositor.
  auto resource_provider = CanvasResourceProvider::CreateWebGPUImageProvider(
      info,
      /*is_origin_top_left=*/true, gpu::SHARED_IMAGE_USAGE_DISPLAY);
  if (!resource_provider)
    return nullptr;

  if (!CopyTextureToResourceProvider(texture, size, resource_provider.get()))
    return nullptr;

  return resource_provider->Snapshot();
}

bool GPUSwapChain::CopyToResourceProvider(
    CanvasResourceProvider* resource_provider) const {
  if (!texture_)
    return false;

  return CopyTextureToResourceProvider(texture_->GetHandle(), Size(),
                                       resource_provider);
}

bool GPUSwapChain::CopyTextureToResourceProvider(
    const WGPUTexture& texture,
    const gfx::Size& size,
    CanvasResourceProvider* resource_provider) const {
  DCHECK(resource_provider);
  DCHECK_EQ(resource_provider->Size(), size);
  DCHECK(resource_provider->GetSharedImageUsageFlags() &
         gpu::SHARED_IMAGE_USAGE_WEBGPU);
  DCHECK(resource_provider->IsOriginTopLeft());

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> shared_context_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (!shared_context_wrapper || !shared_context_wrapper->ContextProvider())
    return false;

  const auto dst_mailbox =
      resource_provider->GetBackingMailboxForOverwrite(kUnverifiedSyncToken);
  if (dst_mailbox.IsZero())
    return false;

  auto* ri = shared_context_wrapper->ContextProvider()->RasterInterface();

  if (!GetContextProviderWeakPtr()) {
    return false;
  }
  // todo(crbug/1267244) Use WebGPUMailboxTexture here instead of doing things
  // manually.
  gpu::webgpu::WebGPUInterface* webgpu =
      GetContextProviderWeakPtr()->ContextProvider()->WebGPUInterface();
  gpu::webgpu::ReservedTexture reservation =
      webgpu->ReserveTexture(device_->GetHandle());
  DCHECK(reservation.texture);

  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  webgpu->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  webgpu->AssociateMailbox(reservation.deviceId, reservation.deviceGeneration,
                           reservation.id, reservation.generation,
                           WGPUTextureUsage_CopyDst,
                           reinterpret_cast<const GLbyte*>(&dst_mailbox));
  WGPUImageCopyTexture source = {
      .nextInChain = nullptr,
      .texture = texture,
      .mipLevel = 0,
      .origin = WGPUOrigin3D{0},
      .aspect = WGPUTextureAspect_All,
  };
  WGPUImageCopyTexture destination = {
      .nextInChain = nullptr,
      .texture = reservation.texture,
      .mipLevel = 0,
      .origin = WGPUOrigin3D{0},
      .aspect = WGPUTextureAspect_All,
  };
  WGPUExtent3D copy_size = {
      .width = static_cast<uint32_t>(size.width()),
      .height = static_cast<uint32_t>(size.height()),
      .depthOrArrayLayers = 1,
  };

  WGPUDawnEncoderInternalUsageDescriptor internal_usage_desc = {
      .chain = {.sType = WGPUSType_DawnEncoderInternalUsageDescriptor},
      .useInternalUsages = true,
  };
  WGPUCommandEncoderDescriptor command_encoder_desc = {
      .nextInChain = &internal_usage_desc.chain,
  };
  WGPUCommandEncoder command_encoder = GetProcs().deviceCreateCommandEncoder(
      device_->GetHandle(), &command_encoder_desc);
  GetProcs().commandEncoderCopyTextureToTexture(command_encoder, &source,
                                                &destination, &copy_size);

  WGPUCommandBuffer command_buffer =
      GetProcs().commandEncoderFinish(command_encoder, nullptr);
  GetProcs().commandEncoderRelease(command_encoder);

  GetProcs().queueSubmit(device_->queue()->GetHandle(), 1u, &command_buffer);
  GetProcs().commandBufferRelease(command_buffer);

  webgpu->DissociateMailbox(reservation.id, reservation.generation);
  GetProcs().textureRelease(reservation.texture);
  webgpu->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  return true;
}

// gpu_swap_chain.idl
GPUTexture* GPUSwapChain::getCurrentTexture() {
  // As we are getting a new texture, if this is an offscreencanvas or if it is
  // going to be presented to video, we have to notify the placeholder or
  // listeners.
  if (context_->IsOffscreenCanvas() ||
      static_cast<HTMLCanvasElement*>(context_->Host())->HasCanvasCapture())
    context_->DidDraw(CanvasPerformanceMonitor::DrawType::kOther);

  // Calling getCurrentTexture returns a texture that is valid until the
  // animation frame it gets presented. If getCurrenTexture is called multiple
  // time, the same texture should be returned. |texture_| is set to null when
  // presented so that we know we should create a new one.
  if (texture_) {
    return texture_;
  }

  if (!swap_buffers_) {
    texture_ = GPUTexture::CreateError(device_);
    return texture_;
  }

  WGPUTexture dawn_client_texture = swap_buffers_->GetNewTexture(size_);
  if (!dawn_client_texture) {
    texture_ = GPUTexture::CreateError(device_);
    return texture_;
  }
  // SwapChain buffer are 2d.
  texture_ = MakeGarbageCollected<GPUTexture>(
      device_, dawn_client_texture, WGPUTextureDimension_2D, format_, usage_);
  return texture_;
}

// WebGPUSwapBufferProvider::Client implementation
void GPUSwapChain::OnTextureTransferred() {
  DCHECK(texture_);
  // The texture is about to be transferred to the compositor.
  // For alpha mode Opaque, clear the alpha channel to 1.0.
  switch (alpha_mode_) {
    case V8GPUCanvasAlphaMode::Enum::kOpaque: {
      WGPUTextureView attachment_view =
          GetProcs().textureCreateView(texture_->GetHandle(), nullptr);

      WGPUDawnEncoderInternalUsageDescriptor internal_usage_desc = {
          .chain = {.sType = WGPUSType_DawnEncoderInternalUsageDescriptor},
          .useInternalUsages = true,
      };
      WGPUCommandEncoderDescriptor command_encoder_desc = {
          .nextInChain = &internal_usage_desc.chain,
      };
      WGPUCommandEncoder command_encoder =
          GetProcs().deviceCreateCommandEncoder(device_->GetHandle(),
                                                &command_encoder_desc);

      WGPURenderPassColorAttachment color_attachment = {
          .view = attachment_view,
          .loadOp = WGPULoadOp_Load,
          .storeOp = WGPUStoreOp_Store,
      };
      WGPURenderPassDescriptor render_pass_desc = {
          .colorAttachmentCount = 1,
          .colorAttachments = &color_attachment,
      };
      WGPURenderPassEncoder pass = GetProcs().commandEncoderBeginRenderPass(
          command_encoder, &render_pass_desc);
      DCHECK(alpha_to_one_pipeline_);
      GetProcs().renderPassEncoderSetPipeline(pass, alpha_to_one_pipeline_);
      GetProcs().renderPassEncoderDraw(pass, 3, 1, 0, 0);
      GetProcs().renderPassEncoderEnd(pass);

      WGPUCommandBuffer command_buffer =
          GetProcs().commandEncoderFinish(command_encoder, nullptr);
      GetProcs().queueSubmit(device_->queue()->GetHandle(), 1, &command_buffer);

      GetProcs().renderPassEncoderRelease(pass);
      GetProcs().commandEncoderRelease(command_encoder);
      GetProcs().commandBufferRelease(command_buffer);
      GetProcs().textureViewRelease(attachment_view);
      break;
    }
    case V8GPUCanvasAlphaMode::Enum::kPremultiplied:
      break;
  }
  texture_ = nullptr;
}

}  // namespace blink
