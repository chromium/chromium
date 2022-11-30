// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"

#include "media/base/video_frame.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_external_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_htmlvideoelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace blink {

namespace {
std::array<float, 12> GetYUVToRGBMatrix(gfx::ColorSpace colorSpace,
                                        size_t bitDepth) {
  // Get the appropriate YUV to RGB conversion matrix.
  SkYUVColorSpace srcSkColorSpace;
  colorSpace.ToSkYUVColorSpace(static_cast<int>(bitDepth), &srcSkColorSpace);
  SkColorMatrix skColorMatrix = SkColorMatrix::YUVtoRGB(srcSkColorSpace);
  float yuvM[20];
  skColorMatrix.getRowMajor(yuvM);
  // Only use columns 1-3 (3x3 conversion matrix) and column 5 (bias values)
  return std::array<float, 12>{yuvM[0],  yuvM[1],  yuvM[2],  yuvM[4],
                               yuvM[5],  yuvM[6],  yuvM[7],  yuvM[9],
                               yuvM[10], yuvM[11], yuvM[12], yuvM[14]};
}

struct ColorSpaceConversionConstants {
  std::array<float, 9> gamutConversionMatrix;
  std::array<float, 7> srcTransferConstants;
  std::array<float, 7> dstTransferConstants;
};

ColorSpaceConversionConstants GetColorSpaceConversionConstants(
    gfx::ColorSpace srcColorSpace,
    gfx::ColorSpace dstColorSpace) {
  ColorSpaceConversionConstants colorSpaceConversionConstants;
  // Get primary matrices for the source and destination color spaces.
  // Multiply the source primary matrix with the inverse destination primary
  // matrix to create a single transformation matrix.
  skcms_Matrix3x3 srcPrimaryMatrixToXYZD50;
  skcms_Matrix3x3 dstPrimaryMatrixToXYZD50;
  srcColorSpace.GetPrimaryMatrix(&srcPrimaryMatrixToXYZD50);
  dstColorSpace.GetPrimaryMatrix(&dstPrimaryMatrixToXYZD50);

  skcms_Matrix3x3 dstPrimaryMatrixFromXYZD50;
  skcms_Matrix3x3_invert(&dstPrimaryMatrixToXYZD50,
                         &dstPrimaryMatrixFromXYZD50);

  skcms_Matrix3x3 transformM = skcms_Matrix3x3_concat(
      &srcPrimaryMatrixToXYZD50, &dstPrimaryMatrixFromXYZD50);
  // From row major matrix to col major matrix
  colorSpaceConversionConstants.gamutConversionMatrix = std::array<float, 9>{
      transformM.vals[0][0], transformM.vals[1][0], transformM.vals[2][0],
      transformM.vals[0][1], transformM.vals[1][1], transformM.vals[2][1],
      transformM.vals[0][2], transformM.vals[1][2], transformM.vals[2][2]};

  // Set constants for source transfer function.
  skcms_TransferFunction src_transfer_fn;
  srcColorSpace.GetInverseTransferFunction(&src_transfer_fn);
  colorSpaceConversionConstants.srcTransferConstants = std::array<float, 7>{
      src_transfer_fn.g, src_transfer_fn.a, src_transfer_fn.b,
      src_transfer_fn.c, src_transfer_fn.d, src_transfer_fn.e,
      src_transfer_fn.f};

  // Set constants for destination transfer function.
  skcms_TransferFunction dst_transfer_fn;
  dstColorSpace.GetTransferFunction(&dst_transfer_fn);
  colorSpaceConversionConstants.dstTransferConstants = std::array<float, 7>{
      dst_transfer_fn.g, dst_transfer_fn.a, dst_transfer_fn.b,
      dst_transfer_fn.c, dst_transfer_fn.d, dst_transfer_fn.e,
      dst_transfer_fn.f};

  return colorSpaceConversionConstants;
}

bool IsSameGamutAndGamma(gfx::ColorSpace srcColorSpace,
                         gfx::ColorSpace dstColorSpace) {
  if (srcColorSpace.GetPrimaryID() == dstColorSpace.GetPrimaryID()) {
    skcms_TransferFunction src;
    skcms_TransferFunction dst;
    if (srcColorSpace.GetTransferFunction(&src) &&
        dstColorSpace.GetTransferFunction(&dst)) {
      return (src.a == dst.a && src.b == dst.b && src.c == dst.c &&
              src.d == dst.d && src.e == dst.e && src.f == dst.f &&
              src.g == dst.g);
    }
  }
  return false;
}

struct ExternalTextureSource {
  scoped_refptr<media::VideoFrame> media_video_frame = nullptr;
  media::PaintCanvasVideoRenderer* video_renderer = nullptr;
  absl::optional<int> media_video_frame_unique_id = absl::nullopt;
  bool valid = false;
};

ExternalTextureSource GetExternalTextureSourceFromVideoElement(
    GPUDevice* device,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  ExternalTextureSource source;

  if (!video) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing video source");
    return source;
  }

  if (video->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "Video element is tainted by cross-origin data and may not be "
        "loaded.");
    return source;
  }

  if (auto* wmp = video->GetWebMediaPlayer()) {
    source.media_video_frame = wmp->GetCurrentFrameThenUpdate();
    source.video_renderer = wmp->GetPaintCanvasVideoRenderer();
  }

  if (!source.media_video_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video "
                                      "element that doesn't have back "
                                      "resource.");
    return source;
  }

  source.media_video_frame_unique_id = source.media_video_frame->unique_id();
  source.valid = true;

  return source;
}

ExternalTextureSource GetExternalTextureSourceFromVideoFrame(
    GPUDevice* device,
    VideoFrame* frame,
    ExceptionState& exception_state) {
  ExternalTextureSource source;

  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing video frame");
    return source;
  }
  // Tainted blink::VideoFrames are not supposed to be possible.
  DCHECK(!frame->WouldTaintOrigin());

  source.media_video_frame = frame->frame();
  if (!source.media_video_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video "
                                      "frame that doesn't have back resource");
    return source;
  }

  if (!source.media_video_frame->coded_size().width() ||
      !source.media_video_frame->coded_size().height()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Cannot import from zero sized video frame");
    return source;
  }

  source.valid = true;

  return source;
}

}  // namespace

// static
GPUExternalTexture* GPUExternalTexture::CreateImpl(
    GPUDevice* device,
    const GPUExternalTextureDescriptor* webgpu_desc,
    scoped_refptr<media::VideoFrame> media_video_frame,
    media::PaintCanvasVideoRenderer* video_renderer,
    absl::optional<int> media_video_frame_unique_id,
    ExceptionState& exception_state) {
  DCHECK(media_video_frame);

  // TODO(crbug.com/1330250): Support additional color spaces for external
  // textures.
  if (webgpu_desc->colorSpace().AsEnum() !=
      V8PredefinedColorSpace::Enum::kSRGB) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "colorSpace !== 'srgb' isn't supported yet.");
    return nullptr;
  }

  PredefinedColorSpace dst_predefined_color_space;
  if (!ValidateAndConvertColorSpace(webgpu_desc->colorSpace(),
                                    dst_predefined_color_space,
                                    exception_state)) {
    return nullptr;
  }

  gfx::ColorSpace srcColorSpace = media_video_frame->ColorSpace();
  // It should be very rare that a frame didn't get a valid colorspace through
  // the guessing process:
  // https://source.chromium.org/chromium/chromium/src/+/main:media/base/video_color_space.cc;l=69;drc=6c9cfff09be8397270b376a4e4407328694e97fa
  // The historical rule for this was to use BT.601 for SD content and BT.709
  // for HD content:
  // https://source.chromium.org/chromium/chromium/src/+/main:media/ffmpeg/ffmpeg_common.cc;l=683;drc=1946212ac0100668f14eb9e2843bdd846e510a1e)
  // We prefer always using BT.709 since SD content in practice is down-scaled
  // HD content, not NTSC broadcast content.
  if (!srcColorSpace.IsValid()) {
    srcColorSpace = gfx::ColorSpace::CreateREC709();
  }
  gfx::ColorSpace dstColorSpace =
      PredefinedColorSpaceToGfxColorSpace(dst_predefined_color_space);

  // TODO(crbug.com/1306753): Use SharedImageProducer and CompositeSharedImage
  // rather than check 'is_webgpu_compatible'.
  bool device_support_zero_copy =
      device->adapter()->features()->FeatureNameSet().Contains(
          "multi-planar-formats");

  if (media_video_frame->HasTextures() &&
      (media_video_frame->format() == media::PIXEL_FORMAT_NV12) &&
      device_support_zero_copy &&
      media_video_frame->metadata().is_webgpu_compatible) {
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
        WebGPUMailboxTexture::FromVideoFrame(
            device->GetDawnControlClient(), device->GetHandle(),
            WGPUTextureUsage::WGPUTextureUsage_TextureBinding,
            media_video_frame);

    WGPUTextureViewDescriptor view_desc = {
        .format = WGPUTextureFormat_R8Unorm,
        .mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED,
        .arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED,
        .aspect = WGPUTextureAspect_Plane0Only};
    WGPUTextureView plane0 = device->GetProcs().textureCreateView(
        mailbox_texture->GetTexture(), &view_desc);
    view_desc.format = WGPUTextureFormat_RG8Unorm;
    view_desc.aspect = WGPUTextureAspect_Plane1Only;
    WGPUTextureView plane1 = device->GetProcs().textureCreateView(
        mailbox_texture->GetTexture(), &view_desc);

    WGPUExternalTextureDescriptor external_texture_desc = {};
    external_texture_desc.plane0 = plane0;
    external_texture_desc.plane1 = plane1;

    external_texture_desc.doYuvToRgbConversionOnly =
        IsSameGamutAndGamma(srcColorSpace, dstColorSpace);

    std::array<float, 12> yuvToRgbMatrix =
        GetYUVToRGBMatrix(srcColorSpace, media_video_frame->BitDepth());
    external_texture_desc.yuvToRgbConversionMatrix = yuvToRgbMatrix.data();

    ColorSpaceConversionConstants colorSpaceConversionConstants =
        GetColorSpaceConversionConstants(srcColorSpace, dstColorSpace);

    external_texture_desc.gamutConversionMatrix =
        colorSpaceConversionConstants.gamutConversionMatrix.data();
    external_texture_desc.srcTransferFunctionParameters =
        colorSpaceConversionConstants.srcTransferConstants.data();
    external_texture_desc.dstTransferFunctionParameters =
        colorSpaceConversionConstants.dstTransferConstants.data();

    GPUExternalTexture* external_texture =
        MakeGarbageCollected<GPUExternalTexture>(
            device,
            device->GetProcs().deviceCreateExternalTexture(
                device->GetHandle(), &external_texture_desc),
            std::move(mailbox_texture), media_video_frame_unique_id);

    // The texture view will be referenced during external texture creation, so
    // by calling release here we ensure this texture view will be destructed
    // when the external texture is destructed.
    device->GetProcs().textureViewRelease(plane0);
    device->GetProcs().textureViewRelease(plane1);

    return external_texture;
  }
  // If the context is lost, the resource provider would be invalid.
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost())
    return nullptr;

  const auto intrinsic_size = media_video_frame->natural_size();

  // Get a recyclable resource for producing WebGPU-compatible shared images.
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      device->GetDawnControlClient()->GetOrCreateCanvasResource(
          SkImageInfo::MakeN32Premul(intrinsic_size.width(),
                                     intrinsic_size.height()),
          /*is_origin_top_left=*/true);
  if (!recyclable_canvas_resource) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  CanvasResourceProvider* resource_provider =
      recyclable_canvas_resource->resource_provider();
  DCHECK(resource_provider);

  viz::RasterContextProvider* raster_context_provider = nullptr;
  if (auto* context_provider = context_provider_wrapper->ContextProvider())
    raster_context_provider = context_provider->RasterContextProvider();

  // TODO(crbug.com/1174809): This isn't efficient for VideoFrames which are
  // already available as a shared image. A WebGPUMailboxTexture should be
  // created directly from the VideoFrame instead.
  // TODO(crbug.com/1174809): VideoFrame cannot extract video_renderer.
  // DrawVideoFrameIntoResourceProvider() creates local_video_renderer always.
  // This might affect performance, maybe a cache local_video_renderer could
  // help.
  const auto dest_rect = gfx::Rect(media_video_frame->natural_size());
  if (!DrawVideoFrameIntoResourceProvider(
          std::move(media_video_frame), resource_provider,
          raster_context_provider, dest_rect, video_renderer)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromCanvasResource(
          device->GetDawnControlClient(), device->GetHandle(),
          WGPUTextureUsage::WGPUTextureUsage_TextureBinding,
          std::move(recyclable_canvas_resource));

  WGPUTextureViewDescriptor view_desc = {};
  view_desc.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
  view_desc.mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
  WGPUTextureView plane0 = device->GetProcs().textureCreateView(
      mailbox_texture->GetTexture(), &view_desc);

  WGPUExternalTextureDescriptor dawn_desc = {};
  dawn_desc.plane0 = plane0;

  ColorSpaceConversionConstants colorSpaceConversionConstants =
      GetColorSpaceConversionConstants(srcColorSpace, dstColorSpace);

  dawn_desc.gamutConversionMatrix =
      colorSpaceConversionConstants.gamutConversionMatrix.data();
  dawn_desc.srcTransferFunctionParameters =
      colorSpaceConversionConstants.srcTransferConstants.data();
  dawn_desc.dstTransferFunctionParameters =
      colorSpaceConversionConstants.dstTransferConstants.data();

  GPUExternalTexture* external_texture =
      MakeGarbageCollected<GPUExternalTexture>(
          device,
          device->GetProcs().deviceCreateExternalTexture(device->GetHandle(),
                                                         &dawn_desc),
          mailbox_texture, media_video_frame_unique_id);

  // The texture view will be referenced during external texture creation, so by
  // calling release here we ensure this texture view will be destructed when
  // the external texture is destructed.
  device->GetProcs().textureViewRelease(plane0);

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::CreateExpired(
    GPUDevice* device,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  // Validate GPUExternalTextureDescriptor.
  ExternalTextureSource source;
  switch (webgpu_desc->source()->GetContentType()) {
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kHTMLVideoElement: {
      HTMLVideoElement* video = webgpu_desc->source()->GetAsHTMLVideoElement();
      source = GetExternalTextureSourceFromVideoElement(device, video,
                                                        exception_state);
      break;
    }
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kVideoFrame: {
      VideoFrame* frame = webgpu_desc->source()->GetAsVideoFrame();
      source = GetExternalTextureSourceFromVideoFrame(device, frame,
                                                      exception_state);
      break;
    }
  }
  if (!source.valid)
    return nullptr;

  // Bypass importing video frame into Dawn.
  GPUExternalTexture* external_texture =
      MakeGarbageCollected<GPUExternalTexture>(
          device,
          device->GetProcs().deviceCreateErrorExternalTexture(
              device->GetHandle()),
          nullptr /*mailbox_texture*/,
          absl::nullopt /*media_video_frame_unique_id*/);

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::FromHTMLVideoElement(
    GPUDevice* device,
    HTMLVideoElement* video,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  ExternalTextureSource source =
      GetExternalTextureSourceFromVideoElement(device, video, exception_state);
  if (!source.valid)
    return nullptr;

  GPUExternalTexture* external_texture = GPUExternalTexture::CreateImpl(
      device, webgpu_desc, source.media_video_frame, source.video_renderer,
      source.media_video_frame_unique_id, exception_state);

  // WebGPU Spec requires that If the latest presented frame of video is not
  // the same frame from which texture was imported, set expired to true and
  // releasing ownership of the underlying resource and remove the texture from
  // active list. Listen to HTMLVideoElement and insert the texture into active
  // list for management.
  if (external_texture) {
    external_texture->ListenToHTMLVideoElement(video);
    device->AddActiveExternalTexture(external_texture);
  }

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::FromVideoFrame(
    GPUDevice* device,
    VideoFrame* frame,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  ExternalTextureSource source =
      GetExternalTextureSourceFromVideoFrame(device, frame, exception_state);
  if (!source.valid)
    return nullptr;

  GPUExternalTexture* external_texture = GPUExternalTexture::CreateImpl(
      device, webgpu_desc, source.media_video_frame, source.video_renderer,
      absl::nullopt, exception_state);

  // If the webcodec video frame has been closed or destroyed, set expired to
  // true, releasing ownership of the underlying resource and remove the texture
  // from active list. Listen to the VideoFrame and insert the texture into
  // active list for management.
  if (external_texture) {
    external_texture->ListenToVideoFrame(frame);

    // VideoFrame maybe closed when GPUExternalTexture trying to listen to.
    // In that case GPUExternalTexture should be expired and GPUDevice
    // doesn't need to manage it.
    if (!external_texture->expired())
      device->AddActiveExternalTexture(external_texture);
  }

  return external_texture;
}

// static
GPUExternalTexture* GPUExternalTexture::Create(
    GPUDevice* device,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  switch (webgpu_desc->source()->GetContentType()) {
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kHTMLVideoElement: {
      HTMLVideoElement* video = webgpu_desc->source()->GetAsHTMLVideoElement();
      return GPUExternalTexture::FromHTMLVideoElement(
          device, video, webgpu_desc, exception_state);
    }
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kVideoFrame: {
      VideoFrame* frame = webgpu_desc->source()->GetAsVideoFrame();
      return GPUExternalTexture::FromVideoFrame(device, frame, webgpu_desc,
                                                exception_state);
    }
  }

  NOTREACHED();
}

GPUExternalTexture::GPUExternalTexture(
    GPUDevice* device,
    WGPUExternalTexture external_texture,
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
    absl::optional<int> media_video_frame_unique_id)
    : DawnObject<WGPUExternalTexture>(device, external_texture),
      mailbox_texture_(mailbox_texture),
      media_video_frame_unique_id_(media_video_frame_unique_id) {
  // Mark GPUExternalTexture without back resources as destroyed because no need
  // to do real resource releasing.
  if (!mailbox_texture_)
    status_ = Status::Destroyed;
}

void GPUExternalTexture::Destroy() {
  DCHECK(!destroyed());
  DCHECK(mailbox_texture_);

  status_ = Status::Destroyed;
  mailbox_texture_.reset();
}

void GPUExternalTexture::ListenToHTMLVideoElement(HTMLVideoElement* video) {
  DCHECK(video);

  video_ = video;
  video->GetDocument()
      .GetScriptedAnimationController()
      .WebGPURegisterVideoFrameStateCallback(WTF::BindRepeating(
          &GPUExternalTexture::ContinueCheckingCurrentVideoFrame,
          WrapPersistent(this)));

  status_ = Status::ListenToHTMLVideoElement;
}

bool GPUExternalTexture::ContinueCheckingCurrentVideoFrame() {
  DCHECK(video_);
  DCHECK(media_video_frame_unique_id_.has_value());

  if (destroyed())
    return false;

  WebMediaPlayer* media_player = video_->GetWebMediaPlayer();

  // HTMLVideoElement transition from having a WMP to not having one.
  if (!media_player) {
    ExpireExternalTextureFromHTMLVideoElement();
    return false;
  }

  // VideoFrame unique id is unique in the same process. Compare the unique id
  // with current video frame from compositor to detect a new presented
  // video frame and expire the GPUExternalTexture.
  if (media_video_frame_unique_id_ != media_player->CurrentFrameId()) {
    ExpireExternalTextureFromHTMLVideoElement();
    return false;
  }

  return true;
}

void GPUExternalTexture::Trace(Visitor* visitor) const {
  visitor->Trace(video_);
  DawnObject<WGPUExternalTexture>::Trace(visitor);
}

void GPUExternalTexture::ExpireExternalTextureFromHTMLVideoElement() {
  DCHECK(status_ != Status::ListenToVideoFrame);
  ExpireExternalTexture();
}

void GPUExternalTexture::ExpireExternalTextureFromVideoFrame() {
  DCHECK(status_ != Status::ListenToHTMLVideoElement);
  ExpireExternalTexture();
}

void GPUExternalTexture::ExpireExternalTexture() {
  device()->RemoveActiveExternalTexture(this);
  Destroy();
}

void GPUExternalTexture::ListenToVideoFrame(VideoFrame* frame) {
  bool success = frame->handle()->WebGPURegisterExternalTextureExpireCallback(
      CrossThreadBindOnce(&GPUExternalTexture::OnVideoFrameClosed,
                          WrapCrossThreadWeakPersistent(this)));
  if (!success) {
    Destroy();
    return;
  }

  task_runner_ =
      device()->GetExecutionContext()->GetTaskRunner(TaskType::kWebGPU);

  status_ = Status::ListenToVideoFrame;
}

void GPUExternalTexture::OnVideoFrameClosed() {
  DCHECK(task_runner_);

  if (destroyed())
    return;

  // Expire the GPUExternalTexture here in the main thread to prevent it from
  // being used again (because WebGPU runs on the main thread). Expiring the
  // texture later in ExpireExternalTextureFromVideoFrame() could occur on a
  // worker thread and cause a race condition.
  status_ = Status::Expired;

  if (task_runner_->BelongsToCurrentThread()) {
    ExpireExternalTextureFromVideoFrame();
    return;
  }

  // If current thread is not the one that creates GPUExternalTexture. Post task
  // to that thread to destroy the GPUExternalTexture.
  task_runner_->PostTask(FROM_HERE,
                         ConvertToBaseOnceCallback(CrossThreadBindOnce(
                             &GPUExternalTexture::OnVideoFrameClosed,
                             WrapCrossThreadWeakPersistent(this))));
}

bool GPUExternalTexture::expired() const {
  return status_ == Status::Expired || status_ == Status::Destroyed;
}

bool GPUExternalTexture::destroyed() const {
  return status_ == Status::Destroyed;
}

}  // namespace blink
