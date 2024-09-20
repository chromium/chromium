// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_graphics_2d_host.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "cc/resources/cross_thread_shared_bitmap.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "content/public/renderer/ppapi_gfx_conversion.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/thunk/enter.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_cftyperef.h"
#endif

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;

namespace content {

namespace {

const int64_t kOffscreenCallbackDelayMs = 1000 / 30;  // 30 fps

// Convert SharedBitmap to SharedImage for pepper.
BASE_FEATURE(kPepperSharedBitmapToSharedImage,
             "PepperSharedBitmapToSharedImage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Converts a rect inside an image of the given dimensions. The rect may be
// NULL to indicate it should be the entire image. If the rect is outside of
// the image, this will do nothing and return false.
bool ValidateAndConvertRect(const PP_Rect* rect,
                            int image_width,
                            int image_height,
                            gfx::Rect* dest) {
  if (!rect) {
    // Use the entire image area.
    *dest = gfx::Rect(0, 0, image_width, image_height);
  } else {
    // Validate the passed-in area.
    if (rect->point.x < 0 || rect->point.y < 0 || rect->size.width <= 0 ||
        rect->size.height <= 0)
      return false;

    // Check the max bounds, being careful of overflow.
    if (static_cast<int64_t>(rect->point.x) +
            static_cast<int64_t>(rect->size.width) >
        static_cast<int64_t>(image_width))
      return false;
    if (static_cast<int64_t>(rect->point.y) +
            static_cast<int64_t>(rect->size.height) >
        static_cast<int64_t>(image_height))
      return false;

    *dest = gfx::Rect(
        rect->point.x, rect->point.y, rect->size.width, rect->size.height);
  }
  return true;
}

// Converts ImageData from PP_IMAGEDATAFORMAT_BGRA_PREMUL to
// PP_IMAGEDATAFORMAT_RGBA_PREMUL, or reverse. It's assumed that the
// destination image is always mapped (so will have non-NULL data).
void ConvertImageData(PPB_ImageData_Impl* src_image,
                      const SkIRect& src_rect,
                      PPB_ImageData_Impl* dest_image,
                      const SkRect& dest_rect) {
  ImageDataAutoMapper auto_mapper(src_image);

  DCHECK(src_image->format() != dest_image->format());
  DCHECK(PPB_ImageData_Impl::IsImageDataFormatSupported(src_image->format()));
  DCHECK(PPB_ImageData_Impl::IsImageDataFormatSupported(dest_image->format()));

  SkBitmap src_bitmap(src_image->GetMappedBitmap());
  SkBitmap dest_bitmap(dest_image->GetMappedBitmap());
  if (src_rect.width() == src_image->width() &&
      dest_rect.width() == dest_image->width()) {
    // Fast path if the full frame can be converted at once.
    SkSwapRB(
        dest_bitmap.getAddr32(static_cast<int>(dest_rect.fLeft),
                              static_cast<int>(dest_rect.fTop)),
        src_bitmap.getAddr32(static_cast<int>(src_rect.fLeft),
                             static_cast<int>(src_rect.fTop)),
        src_rect.width() * src_rect.height());
  } else {
    // Slow path where we convert line by line.
    for (int y = 0; y < src_rect.height(); y++) {
      SkSwapRB(
          dest_bitmap.getAddr32(static_cast<int>(dest_rect.fLeft),
                                static_cast<int>(dest_rect.fTop + y)),
          src_bitmap.getAddr32(static_cast<int>(src_rect.fLeft),
                               static_cast<int>(src_rect.fTop + y)),
          src_rect.width());
    }
  }
}

}  // namespace

PepperGraphics2DHost::SharedImageInfo::SharedImageInfo(
    gpu::SyncToken sync_token,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    gfx::Size size)
    : sync_token(sync_token),
      shared_image(std::move(shared_image)),
      size(size) {}

PepperGraphics2DHost::SharedImageInfo::SharedImageInfo(
    const SharedImageInfo& shared_image_info) = default;

PepperGraphics2DHost::SharedImageInfo::~SharedImageInfo() = default;

struct PepperGraphics2DHost::QueuedOperation {
  enum Type { PAINT, SCROLL, REPLACE, TRANSFORM };

  QueuedOperation(Type t)
      : type(t), paint_x(0), paint_y(0), scroll_dx(0), scroll_dy(0) {}

  Type type;

  // Valid when type == PAINT.
  scoped_refptr<PPB_ImageData_Impl> paint_image;
  int paint_x, paint_y;
  gfx::Rect paint_src_rect;

  // Valid when type == SCROLL.
  gfx::Rect scroll_clip_rect;
  int scroll_dx, scroll_dy;

  // Valid when type == REPLACE.
  scoped_refptr<PPB_ImageData_Impl> replace_image;

  // Valid when type == TRANSFORM
  float scale;
  gfx::PointF translation;
};

// static
PepperGraphics2DHost* PepperGraphics2DHost::Create(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const PP_Size& size,
    PP_Bool is_always_opaque,
    scoped_refptr<PPB_ImageData_Impl> backing_store) {
  PepperGraphics2DHost* resource_host =
      new PepperGraphics2DHost(host, instance, resource);
  if (!resource_host->Init(size.width,
                           size.height,
                           PP_ToBool(is_always_opaque),
                           backing_store)) {
    delete resource_host;
    return nullptr;
  }
  return resource_host;
}

PepperGraphics2DHost::PepperGraphics2DHost(RendererPpapiHost* host,
                                           PP_Instance instance,
                                           PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      bound_instance_(nullptr),
      need_flush_ack_(false),
      offscreen_flush_pending_(false),
      is_always_opaque_(false),
      scale_(1.0f),
      is_running_in_process_(host->IsRunningInProcess()),
      enable_gpu_memory_buffer_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              blink::switches::kEnableGpuMemoryBufferCompositorResources)) {}

PepperGraphics2DHost::~PepperGraphics2DHost() {
  // Delete textures owned by PepperGraphics2DHost, but not those sent to the
  // compositor, since those will be deleted by ReleaseTextureCallback() when it
  // runs.
  while (main_thread_context_ && !recycled_shared_images_.empty()) {
    auto& shared_image_info = recycled_shared_images_.back();
    main_thread_context_->SharedImageInterface()->DestroySharedImage(
        shared_image_info.sync_token,
        std::move(shared_image_info.shared_image));
    recycled_shared_images_.pop_back();
  }

  // Unbind from the instance when destroyed if we're still bound.
  if (bound_instance_)
    bound_instance_->BindGraphics(bound_instance_->pp_instance(), 0);
}

bool PepperGraphics2DHost::Init(
    int width,
    int height,
    bool is_always_opaque,
    scoped_refptr<PPB_ImageData_Impl> backing_store) {
  // The underlying PPB_ImageData_Impl will validate the dimensions.
  image_data_ = backing_store;
  if (!image_data_->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
                         width,
                         height,
                         true) ||
      !image_data_->Map()) {
    image_data_ = nullptr;
    return false;
  }
  is_always_opaque_ = is_always_opaque;
  scale_ = 1.0f;

  return true;
}

int32_t PepperGraphics2DHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperGraphics2DHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_PaintImageData,
                                      OnHostMsgPaintImageData)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_Scroll,
                                      OnHostMsgScroll)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_ReplaceContents,
                                      OnHostMsgReplaceContents)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Graphics2D_Flush,
                                        OnHostMsgFlush)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_SetScale,
                                      OnHostMsgSetScale)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_SetLayerTransform,
                                      OnHostMsgSetLayerTransform)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Graphics2D_ReadImageData,
                                      OnHostMsgReadImageData)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperGraphics2DHost::IsGraphics2DHost() { return true; }

bool PepperGraphics2DHost::ReadImageData(PP_Resource image,
                                         const PP_Point* top_left) {
  // Get and validate the image object to paint into.
  EnterResourceNoLock<PPB_ImageData_API> enter(image, true);
  if (enter.failed())
    return false;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());
  if (!PPB_ImageData_Impl::IsImageDataFormatSupported(image_resource->format()))
    return false;  // Must be in the right format.

  // Validate the bitmap position.
  int x = top_left->x;
  if (x < 0 ||
      static_cast<int64_t>(x) + static_cast<int64_t>(image_resource->width()) >
          image_data_->width())
    return false;
  int y = top_left->y;
  if (y < 0 ||
      static_cast<int64_t>(y) + static_cast<int64_t>(image_resource->height()) >
          image_data_->height())
    return false;

  ImageDataAutoMapper auto_mapper(image_resource);
  if (!auto_mapper.is_valid())
    return false;

  SkIRect src_irect = {x, y, x + image_resource->width(),
                       y + image_resource->height()};
  SkRect dest_rect = {SkIntToScalar(0), SkIntToScalar(0),
                      SkIntToScalar(image_resource->width()),
                      SkIntToScalar(image_resource->height())};

  if (image_resource->format() != image_data_->format()) {
    // Convert the image data if the format does not match.
    ConvertImageData(image_data_.get(), src_irect, image_resource, dest_rect);
  } else {
    SkCanvas* dest_canvas = image_resource->GetCanvas();

    // We want to replace the contents of the bitmap rather than blend.
    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    dest_canvas->drawImageRect(image_data_->GetMappedBitmap().asImage(),
                               SkRect::Make(src_irect), dest_rect,
                               SkSamplingOptions(), &paint,
                               SkCanvas::kStrict_SrcRectConstraint);
  }
  return true;
}

bool PepperGraphics2DHost::BindToInstance(
    PepperPluginInstanceImpl* new_instance) {
  if (new_instance && new_instance->pp_instance() != pp_instance())
    return false;  // Can't bind other instance's contexts.
  if (bound_instance_ == new_instance)
    return true;  // Rebinding the same device, nothing to do.
  if (bound_instance_ && new_instance)
    return false;  // Can't change a bound device.

  if (!new_instance) {
    // When the device is detached, we'll not get any more paint callbacks so
    // we need to clear the list, but we still want to issue any pending
    // callbacks to the plugin.
    if (need_flush_ack_)
      ScheduleOffscreenFlushAck();
  } else {
    // Devices being replaced, redraw the plugin.
    new_instance->InvalidateRect(gfx::Rect());
  }

  cached_bitmap_ = nullptr;
  cached_bitmap_registration_ = cc::SharedBitmapIdRegistration();
  composited_output_modified_ = true;

  bound_instance_ = new_instance;
  return true;
}

// The |backing_bitmap| must be clipped to the |plugin_rect| to avoid painting
// outside the plugin area. This can happen if the plugin has been resized since
// PaintImageData verified the image is within the plugin size.
void PepperGraphics2DHost::Paint(cc::PaintCanvas* canvas,
                                 const gfx::Rect& plugin_rect,
                                 const gfx::Rect& paint_rect) {
  TRACE_EVENT0("pepper", "PepperGraphics2DHost::Paint");
  ImageDataAutoMapper auto_mapper(image_data_.get());
  SkBitmap backing_bitmap = image_data_->GetMappedBitmap();

  gfx::Rect invalidate_rect = plugin_rect;
  invalidate_rect.Intersect(paint_rect);
  SkRect sk_invalidate_rect = gfx::RectToSkRect(invalidate_rect);
  cc::PaintCanvasAutoRestore auto_restore(canvas, true);
  canvas->clipRect(sk_invalidate_rect);
  gfx::Size pixel_image_size(image_data_->width(), image_data_->height());
  gfx::Size image_size = gfx::ScaleToFlooredSize(pixel_image_size, scale_);

  PepperPluginInstance* plugin_instance =
      renderer_ppapi_host_->GetPluginInstance(pp_instance());
  if (!plugin_instance)
    return;
  if (plugin_instance->IsFullPagePlugin()) {
    // When we're resizing a window with a full-frame plugin, the plugin may
    // not yet have bound a new device, which will leave parts of the
    // background exposed if the window is getting larger. We want this to
    // show white (typically less jarring) rather than black or uninitialized.
    // We don't do this for non-full-frame plugins since we specifically want
    // the page background to show through.
    cc::PaintCanvasAutoRestore full_page_auto_restore(canvas, true);
    SkRect image_data_rect =
        gfx::RectToSkRect(gfx::Rect(plugin_rect.origin(), image_size));
    canvas->clipRect(image_data_rect, SkClipOp::kDifference);

    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setColor(SK_ColorWHITE);
    canvas->drawRect(sk_invalidate_rect, flags);
  }

  cc::PaintFlags flags;
  if (is_always_opaque_) {
    // When we know the device is opaque, we can disable blending for slightly
    // more optimized painting.
    flags.setBlendMode(SkBlendMode::kSrc);
  }

  SkPoint pixel_origin(PointToSkPoint(plugin_rect.origin()));
  if (scale_ != 1.0f && scale_ > 0.0f) {
    canvas->scale(scale_, scale_);
    pixel_origin.scale(1.0f / scale_);
  }
  // TODO(khushalsagar): Can this be cached on image_data_, and invalidated when
  // the bitmap changes?
  canvas->drawImage(cc::PaintImage::CreateFromBitmap(std::move(backing_bitmap)),
                    pixel_origin.x(), pixel_origin.y(), SkSamplingOptions(),
                    &flags);
}

void PepperGraphics2DHost::ViewInitiatedPaint() {
  TRACE_EVENT0("pepper", "PepperGraphics2DHost::ViewInitiatedPaint");
  if (need_flush_ack_) {
    SendFlushAck();
    need_flush_ack_ = false;
  }
}

float PepperGraphics2DHost::GetScale() const { return scale_; }

bool PepperGraphics2DHost::IsAlwaysOpaque() const { return is_always_opaque_; }

gfx::Size PepperGraphics2DHost::Size() const {
  if (!image_data_.get())
    return gfx::Size();
  return gfx::Size(image_data_->width(), image_data_->height());
}

void PepperGraphics2DHost::ClearCache() {
  cached_bitmap_ = nullptr;
  cached_bitmap_registration_ = cc::SharedBitmapIdRegistration();
}

int32_t PepperGraphics2DHost::OnHostMsgPaintImageData(
    ppapi::host::HostMessageContext* context,
    const ppapi::HostResource& image_data,
    const PP_Point& top_left,
    bool src_rect_specified,
    const PP_Rect& src_rect) {
  EnterResourceNoLock<PPB_ImageData_API> enter(image_data.host_resource(),
                                               true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  QueuedOperation operation(QueuedOperation::PAINT);
  operation.paint_image = image_resource;
  if (!ValidateAndConvertRect(src_rect_specified ? &src_rect : nullptr,
                              image_resource->width(), image_resource->height(),
                              &operation.paint_src_rect))
    return PP_ERROR_BADARGUMENT;

  // Validate the bitmap position using the previously-validated rect, there
  // should be no painted area outside of the image.
  int64_t x64 = static_cast<int64_t>(top_left.x);
  int64_t y64 = static_cast<int64_t>(top_left.y);
  if (x64 + static_cast<int64_t>(operation.paint_src_rect.x()) < 0 ||
      x64 + static_cast<int64_t>(operation.paint_src_rect.right()) >
          image_data_->width())
    return PP_ERROR_BADARGUMENT;
  if (y64 + static_cast<int64_t>(operation.paint_src_rect.y()) < 0 ||
      y64 + static_cast<int64_t>(operation.paint_src_rect.bottom()) >
          image_data_->height())
    return PP_ERROR_BADARGUMENT;
  operation.paint_x = top_left.x;
  operation.paint_y = top_left.y;

  queued_operations_.push_back(operation);
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgScroll(
    ppapi::host::HostMessageContext* context,
    bool clip_specified,
    const PP_Rect& clip,
    const PP_Point& amount) {
  QueuedOperation operation(QueuedOperation::SCROLL);
  if (!ValidateAndConvertRect(clip_specified ? &clip : nullptr,
                              image_data_->width(), image_data_->height(),
                              &operation.scroll_clip_rect))
    return PP_ERROR_BADARGUMENT;

  // If we're being asked to scroll by more than the clip rect size, just
  // ignore this scroll command and say it worked.
  int32_t dx = amount.x;
  int32_t dy = amount.y;
  if (dx <= -image_data_->width() || dx >= image_data_->width() ||
      dy <= -image_data_->height() || dy >= image_data_->height())
    return PP_ERROR_BADARGUMENT;

  operation.scroll_dx = dx;
  operation.scroll_dy = dy;

  queued_operations_.push_back(operation);
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgReplaceContents(
    ppapi::host::HostMessageContext* context,
    const ppapi::HostResource& image_data) {
  EnterResourceNoLock<PPB_ImageData_API> enter(image_data.host_resource(),
                                               true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_ImageData_Impl* image_resource =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  if (!PPB_ImageData_Impl::IsImageDataFormatSupported(image_resource->format()))
    return PP_ERROR_BADARGUMENT;

  if (image_resource->width() != image_data_->width() ||
      image_resource->height() != image_data_->height())
    return PP_ERROR_BADARGUMENT;

  QueuedOperation operation(QueuedOperation::REPLACE);
  operation.replace_image = image_resource;
  queued_operations_.push_back(operation);
  return PP_OK;
}

int32_t PepperGraphics2DHost::OnHostMsgFlush(
    ppapi::host::HostMessageContext* context) {
  // Don't allow more than one pending flush at a time.
  if (HasPendingFlush())
    return PP_ERROR_INPROGRESS;

  PP_Resource old_image_data = 0;
  flush_reply_context_ = context->MakeReplyMessageContext();
  if (is_running_in_process_)
    return Flush(nullptr);

  // Reuse image data when running out of process.
  int32_t result = Flush(&old_image_data);

  if (old_image_data) {
    // If the Graphics2D has an old image data it's not using any more, send
    // it back to the plugin for possible re-use. See ppb_image_data_proxy.cc
    // for a description how this process works.
    ppapi::HostResource old_image_data_host_resource;
    old_image_data_host_resource.SetHostResource(pp_instance(), old_image_data);
    host()->Send(new PpapiMsg_PPBImageData_NotifyUnusedImageData(
        ppapi::API_ID_PPB_IMAGE_DATA, old_image_data_host_resource));
  }

  return result;
}

int32_t PepperGraphics2DHost::OnHostMsgSetScale(
    ppapi::host::HostMessageContext* context,
    float scale) {
  if (scale > 0.0f) {
    scale_ = scale;
    return PP_OK;
  }
  return PP_ERROR_BADARGUMENT;
}

int32_t PepperGraphics2DHost::OnHostMsgSetLayerTransform(
            ppapi::host::HostMessageContext* context,
            float scale,
            const PP_FloatPoint& translation) {
  if (scale < 0.0f)
    return PP_ERROR_BADARGUMENT;

  QueuedOperation operation(QueuedOperation::TRANSFORM);
  operation.scale = scale;
  operation.translation = gfx::PointF(translation.x, translation.y);
  queued_operations_.push_back(operation);
  return PP_OK;
}


int32_t PepperGraphics2DHost::OnHostMsgReadImageData(
    ppapi::host::HostMessageContext* context,
    PP_Resource image,
    const PP_Point& top_left) {
  context->reply_msg = PpapiPluginMsg_Graphics2D_ReadImageDataAck();
  return ReadImageData(image, &top_left) ? PP_OK : PP_ERROR_FAILED;
}

void PepperGraphics2DHost::ReleaseSoftwareCallback(
    scoped_refptr<cc::CrossThreadSharedBitmap> bitmap,
    cc::SharedBitmapIdRegistration registration,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    scoped_refptr<gpu::SharedImageInterface> shared_image_interface,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  shared_image->UpdateDestructionSyncToken(sync_token);

  // Only keep around a cached bitmap if the plugin is currently drawing (has
  // need_flush_ack_ set).
  if (need_flush_ack_ && bound_instance_) {
    cached_bitmap_ = std::move(bitmap);
    cached_bitmap_registration_ = std::move(registration);
    cached_bitmap_shared_image_ = std::move(shared_image);
    cached_bitmap_shared_image_interface_ = std::move(shared_image_interface);
  } else {
    cached_bitmap_ = nullptr;
    cached_bitmap_registration_ = cc::SharedBitmapIdRegistration();
    cached_bitmap_shared_image_ = nullptr;
    cached_bitmap_shared_image_interface_ = nullptr;
  }
}

// static
void PepperGraphics2DHost::ReleaseTextureCallback(
    base::WeakPtr<PepperGraphics2DHost> host,
    scoped_refptr<viz::RasterContextProvider> context,
    const gfx::Size& size,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    bool lost) {
  // Only recycle shared images from the same context, otherwise they may be
  // lost.
  if (host && !lost && context == host->main_thread_context_) {
    host->recycled_shared_images_.emplace_back(sync_token,
                                               std::move(shared_image), size);
    return;
  }

  context->SharedImageInterface()->DestroySharedImage(sync_token,
                                                      std::move(shared_image));
}

bool PepperGraphics2DHost::PrepareTransferableResource(
    cc::SharedBitmapIdRegistrar* bitmap_registrar,
    viz::TransferableResource* transferable_resource,
    viz::ReleaseCallback* release_callback) {
  // Reuse the |main_thread_context_| if it is not lost. If it is lost, we can't
  // reuse the shared images, they are invalid. If the compositing mode changed,
  // the context will be lost also, so we get both together.
  if (!main_thread_context_ ||
      main_thread_context_->RasterInterface()->GetGraphicsResetStatusKHR() !=
          GL_NO_ERROR) {
    recycled_shared_images_.clear();
    main_thread_context_ = nullptr;

    if (!is_gpu_compositing_disabled_) {
      RenderThreadImpl* rti = RenderThreadImpl::current();
      is_gpu_compositing_disabled_ = rti->IsGpuCompositingDisabled();
      if (!is_gpu_compositing_disabled_) {
        // Using gpu compositing.
        main_thread_context_ = rti->SharedMainThreadContextProvider();
      }

      // The last transferable resource is invalid, either coming from a lost
      // context or because we switched to software compositing. Force us to
      // send the frame to the compositor again even if not changed.
      composited_output_modified_ = true;
    }
  }

  if (!composited_output_modified_)
    return false;

  // Context creation failed, so we're unable to give this frame to the
  // compositor. Try again next time.
  if (!is_gpu_compositing_disabled_ && !main_thread_context_)
    return false;

  // When gpu compositing, the compositor expects gpu resources, so we copy the
  // |image_data_| into a texture.
  if (main_thread_context_) {
    auto* ri = main_thread_context_->RasterInterface();
    auto* sii = main_thread_context_->SharedImageInterface();

    // The bitmap in |image_data_| uses the skia N32 byte order.
    constexpr bool bitmap_is_bgra = kN32_SkColorType == kBGRA_8888_SkColorType;
    const bool texture_can_be_bgra =
        main_thread_context_->ContextCapabilities().texture_format_bgra8888;
    const bool upload_bgra = bitmap_is_bgra && texture_can_be_bgra;
    const viz::SharedImageFormat format =
        upload_bgra ? viz::SinglePlaneFormat::kBGRA_8888
                    : viz::SinglePlaneFormat::kRGBA_8888;

    bool overlays_supported = enable_gpu_memory_buffer_ &&
                              main_thread_context_->SharedImageInterface()
                                  ->GetCapabilities()
                                  .supports_scanout_shared_images;

    const gfx::Size size(image_data_->width(), image_data_->height());

    scoped_refptr<gpu::ClientSharedImage> shared_image;
    gpu::SyncToken in_sync_token;
    while (!recycled_shared_images_.empty()) {
      auto& shared_image_info = recycled_shared_images_.back();
      if (shared_image_info.size == size) {
        in_sync_token = shared_image_info.sync_token;
        shared_image = std::move(shared_image_info.shared_image);
        recycled_shared_images_.pop_back();
        break;
      }
      sii->DestroySharedImage(shared_image_info.sync_token,
                              std::move(shared_image_info.shared_image));
      recycled_shared_images_.pop_back();
    }
    if (!shared_image) {
      // We will potentially write to this SharedImage via the raster interface
      // (which might be going over GLES2) and will later send it off to the
      // display compositor.
      gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_GLES2_WRITE |
                                       gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
      if (overlays_supported)
        usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
      shared_image = sii->CreateSharedImage(
          {format, size, gfx::ColorSpace(), usage, "PepperGraphics2DHost"},
          gpu::kNullSurfaceHandle);
      CHECK(shared_image);
      in_sync_token = sii->GenUnverifiedSyncToken();
    }

    void* src = image_data_->Map();

    // Convert to RGBA if we can't upload BGRA. This is slow sad times.
    std::unique_ptr<uint32_t[]> swizzled;
    if (bitmap_is_bgra != upload_bgra) {
      size_t num_pixels = (base::CheckedNumeric<size_t>(image_data_->width()) *
                           image_data_->height())
                              .ValueOrDie();
      swizzled = std::make_unique<uint32_t[]>(num_pixels);
      SkSwapRB(swizzled.get(), static_cast<uint32_t*>(src), num_pixels);
      src = swizzled.get();
    }

    SkImageInfo src_info = SkImageInfo::Make(
        size.width(), size.height(), viz::ToClosestSkColorType(true, format),
        kUnknown_SkAlphaType);
    ri->WaitSyncTokenCHROMIUM(in_sync_token.GetConstData());

    uint32_t texture_target = shared_image->GetTextureTarget();

    ri->WritePixels(shared_image->mailbox(), /*dst_x_offset=*/0,
                    /*dst_y_offset=*/0, texture_target,
                    SkPixmap(src_info, src, src_info.minRowBytes()));

    gpu::SyncToken out_sync_token;
    ri->GenUnverifiedSyncTokenCHROMIUM(out_sync_token.GetData());

    image_data_->Unmap();
    swizzled.reset();

    gpu::Mailbox gpu_mailbox = shared_image->mailbox();
    *release_callback =
        base::BindOnce(&ReleaseTextureCallback, weak_ptr_factory_.GetWeakPtr(),
                       main_thread_context_, size, std::move(shared_image));
    *transferable_resource = viz::TransferableResource::MakeGpu(
        std::move(gpu_mailbox), texture_target, std::move(out_sync_token), size,
        format, overlays_supported,
        viz::TransferableResource::ResourceSource::kPepperGraphics2D);
    composited_output_modified_ = false;
    return true;
  }

  gfx::Size pixel_image_size(image_data_->width(), image_data_->height());
  scoped_refptr<cc::CrossThreadSharedBitmap> shared_bitmap;
  cc::SharedBitmapIdRegistration registration;
  scoped_refptr<gpu::ClientSharedImage> shared_image;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface =
      RenderThreadImpl::current()->GetRenderThreadSharedImageInterface();

  if (base::FeatureList::IsEnabled(kPepperSharedBitmapToSharedImage)) {
    if (cached_bitmap_) {
      // |shared_image_interface| changes after the gpu channel is lost.
      // Reuse the shared bitmap only when sii remains the same.
      if (cached_bitmap_->size() == pixel_image_size &&
          shared_image_interface == cached_bitmap_shared_image_interface_) {
        shared_bitmap = std::move(cached_bitmap_);
        shared_image = std::move(cached_bitmap_shared_image_);
      } else {
        cached_bitmap_ = nullptr;
        cached_bitmap_shared_image_ = nullptr;
        cached_bitmap_shared_image_interface_ = nullptr;
      }
    }

    // Without shared_image_interface, we won't be able to prepare the resources
    // for the compositor. Will retry next time.
    if (!shared_image_interface) {
      return false;
    }

    if (!shared_bitmap) {
      auto shared_image_mapping = shared_image_interface->CreateSharedImage(
          {viz::SinglePlaneFormat::kBGRA_8888, pixel_image_size,
           gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_CPU_WRITE,
           "PepperGraphics2DSharedBitmap"});

      shared_bitmap = base::MakeRefCounted<cc::CrossThreadSharedBitmap>(
          viz::SharedBitmapId(), base::ReadOnlySharedMemoryRegion(),
          std::move(shared_image_mapping.mapping), pixel_image_size,
          viz::SinglePlaneFormat::kBGRA_8888);
      shared_image = std::move(shared_image_mapping.shared_image);
    }

    gpu::SyncToken sync_token = shared_image_interface->GenVerifiedSyncToken();
    *transferable_resource = viz::TransferableResource::MakeSoftwareSharedImage(
        shared_image, sync_token, pixel_image_size,
        viz::SinglePlaneFormat::kBGRA_8888,
        viz::TransferableResource::ResourceSource::kPepperGraphics2D);

    void* src = image_data_->Map();
    memcpy(shared_bitmap->memory(), src,
           viz::ResourceSizes::CheckedSizeInBytes<size_t>(
               pixel_image_size, viz::SinglePlaneFormat::kBGRA_8888));
    image_data_->Unmap();

    *release_callback =
        base::BindOnce(&PepperGraphics2DHost::ReleaseSoftwareCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(shared_bitmap),
                       cc::SharedBitmapIdRegistration(),
                       std::move(shared_image), shared_image_interface);
  } else {
    if (cached_bitmap_) {
      if (cached_bitmap_->size() == pixel_image_size) {
        shared_bitmap = std::move(cached_bitmap_);
        registration = std::move(cached_bitmap_registration_);
      } else {
        cached_bitmap_ = nullptr;
        cached_bitmap_registration_ = cc::SharedBitmapIdRegistration();
      }
    }

    if (!shared_bitmap) {
      viz::SharedBitmapId id = viz::SharedBitmap::GenerateId();
      base::MappedReadOnlyRegion shm =
          viz::bitmap_allocation::AllocateSharedBitmap(
              pixel_image_size, viz::SinglePlaneFormat::kRGBA_8888);
      shared_bitmap = base::MakeRefCounted<cc::CrossThreadSharedBitmap>(
          id, std::move(shm.region), std::move(shm.mapping), pixel_image_size,
          viz::SinglePlaneFormat::kRGBA_8888);
      registration =
          bitmap_registrar->RegisterSharedBitmapId(id, shared_bitmap);
    }
    void* src = image_data_->Map();
    memcpy(shared_bitmap->memory(), src,
           viz::ResourceSizes::CheckedSizeInBytes<size_t>(
               pixel_image_size, viz::SinglePlaneFormat::kRGBA_8888));
    image_data_->Unmap();

    *transferable_resource =
        viz::TransferableResource::MakeSoftwareSharedBitmap(
            shared_bitmap->id(), gpu::SyncToken(), pixel_image_size,
            viz::SinglePlaneFormat::kRGBA_8888,
            viz::TransferableResource::ResourceSource::kPepperGraphics2D);

    *release_callback =
        base::BindOnce(&PepperGraphics2DHost::ReleaseSoftwareCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(shared_bitmap),
                       std::move(registration), /*shared_image=*/nullptr,
                       /*shared_image_interface=*/nullptr);
  }

  composited_output_modified_ = false;

  return true;
}

void PepperGraphics2DHost::AttachedToNewLayer() {
  composited_output_modified_ = true;
}

int32_t PepperGraphics2DHost::Flush(PP_Resource* old_image_data) {
  bool done_replace_contents = false;
  bool no_update_visible = true;
  bool is_plugin_visible = true;

  for (size_t i = 0; i < queued_operations_.size(); i++) {
    QueuedOperation& operation = queued_operations_[i];
    gfx::Rect op_rect;
    switch (operation.type) {
      case QueuedOperation::TRANSFORM:
        ExecuteTransform(operation.scale, operation.translation, &op_rect);
        break;
      case QueuedOperation::PAINT:
        ExecutePaintImageData(operation.paint_image.get(),
                              operation.paint_x,
                              operation.paint_y,
                              operation.paint_src_rect,
                              &op_rect);
        break;
      case QueuedOperation::SCROLL:
        ExecuteScroll(operation.scroll_clip_rect,
                      operation.scroll_dx,
                      operation.scroll_dy,
                      &op_rect);
        break;
      case QueuedOperation::REPLACE:
        // Since the out parameter |old_image_data| takes ownership of the
        // reference, if there are more than one ReplaceContents calls queued
        // the first |old_image_data| will get overwritten and leaked. So we
        // only supply this for the first call.
        ExecuteReplaceContents(
            operation.replace_image.get(), &op_rect,
            done_replace_contents ? nullptr : old_image_data);
        done_replace_contents = true;
        break;
    }

    // For correctness with accelerated compositing, we must issue an invalidate
    // on the full op_rect even if it is partially or completely off-screen.
    // However, if we issue an invalidate for a clipped-out region, WebKit will
    // do nothing and we won't get any ViewFlushedPaint calls, leaving our
    // callback stranded. So we still need to check whether the repainted area
    // is visible to determine how to deal with the callback.
    if (bound_instance_ && !op_rect.IsEmpty()) {
      gfx::Point scroll_delta(operation.scroll_dx, operation.scroll_dy);
      // In use-zoom-for-dsf mode, the viewport (thus cc) uses native
      // pixels, so the damage and rects have to be scaled.
      gfx::Rect op_rect_in_viewport = op_rect;
      ConvertToLogicalPixels(scale_, &op_rect, nullptr);
      if (!ConvertToLogicalPixels(scale_ / viewport_to_dip_scale_,
                                  &op_rect_in_viewport,
                                  operation.type == QueuedOperation::SCROLL
                                      ? &scroll_delta
                                      : nullptr)) {
        // Conversion requires falling back to InvalidateRect.
        operation.type = QueuedOperation::PAINT;
      }

      gfx::Rect clip = PP_ToGfxRect(bound_instance_->view_data().clip_rect);
      is_plugin_visible = !clip.IsEmpty();

      // Set |no_update_visible| to false if the change overlaps the visible
      // area.
      if (!gfx::IntersectRects(clip, op_rect).IsEmpty()) {
        no_update_visible = false;
      }

      if (!op_rect_in_viewport.IsEmpty())
        bound_instance_->InvalidateRect(op_rect_in_viewport);
      composited_output_modified_ = true;
    }
  }
  queued_operations_.clear();

  if (!bound_instance_) {
    // As promised in the API, we always schedule callback when unbound.
    ScheduleOffscreenFlushAck();
  } else if (no_update_visible && is_plugin_visible &&
             bound_instance_->view_data().is_page_visible) {
    // There's nothing visible to invalidate so just schedule the callback to
    // execute in the next round of the message loop.
    ScheduleOffscreenFlushAck();
  } else {
    need_flush_ack_ = true;
  }

  return PP_OK_COMPLETIONPENDING;
}

void PepperGraphics2DHost::ExecuteTransform(const float& scale,
                                            const gfx::PointF& translate,
                                            gfx::Rect* invalidated_rect) {
  if (bound_instance_) {
    bound_instance_->SetGraphics2DTransform(scale, translate);
    *invalidated_rect =
        gfx::Rect(0, 0, image_data_->width(), image_data_->height());
  }
}

void PepperGraphics2DHost::ExecutePaintImageData(PPB_ImageData_Impl* image,
                                                 int x,
                                                 int y,
                                                 const gfx::Rect& src_rect,
                                                 gfx::Rect* invalidated_rect) {
  // Ensure the source image is mapped to read from it.
  ImageDataAutoMapper auto_mapper(image);
  if (!auto_mapper.is_valid())
    return;

  // Portion within the source image to cut out.
  SkIRect src_irect = {src_rect.x(), src_rect.y(), src_rect.right(),
                       src_rect.bottom()};

  // Location within the backing store to copy to.
  *invalidated_rect = src_rect;
  invalidated_rect->Offset(x, y);
  SkRect dest_rect = {SkIntToScalar(invalidated_rect->x()),
                      SkIntToScalar(invalidated_rect->y()),
                      SkIntToScalar(invalidated_rect->right()),
                      SkIntToScalar(invalidated_rect->bottom())};

  if (image->format() != image_data_->format()) {
    // Convert the image data if the format does not match.
    ConvertImageData(image, src_irect, image_data_.get(), dest_rect);
  } else {
    // We're guaranteed to have a mapped canvas since we mapped it in Init().
    SkCanvas* backing_canvas = image_data_->GetCanvas();

    // We want to replace the contents of the bitmap rather than blend.
    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    backing_canvas->drawImageRect(
        image->GetMappedBitmap().asImage(), SkRect::Make(src_irect), dest_rect,
        SkSamplingOptions(), &paint, SkCanvas::kStrict_SrcRectConstraint);
  }
}

void PepperGraphics2DHost::ExecuteScroll(const gfx::Rect& clip,
                                         int dx,
                                         int dy,
                                         gfx::Rect* invalidated_rect) {
  gfx::ScrollCanvas(image_data_->GetCanvas(), clip, gfx::Vector2d(dx, dy));
  *invalidated_rect = clip;
}

void PepperGraphics2DHost::ExecuteReplaceContents(PPB_ImageData_Impl* image,
                                                  gfx::Rect* invalidated_rect,
                                                  PP_Resource* old_image_data) {
  if (image->format() != image_data_->format()) {
    DCHECK(image->width() == image_data_->width() &&
           image->height() == image_data_->height());
    // Convert the image data if the format does not match.
    SkIRect src_irect = {0, 0, image->width(), image->height()};
    SkRect dest_rect = {SkIntToScalar(0), SkIntToScalar(0),
                        SkIntToScalar(image_data_->width()),
                        SkIntToScalar(image_data_->height())};
    ConvertImageData(image, src_irect, image_data_.get(), dest_rect);
  } else {
    // The passed-in image may not be mapped in our process, and we need to
    // guarantee that the current backing store is always mapped.
    if (!image->Map())
      return;

    if (old_image_data)
      *old_image_data = image_data_->GetReference();
    image_data_ = image;
  }
  *invalidated_rect =
      gfx::Rect(0, 0, image_data_->width(), image_data_->height());
}

void PepperGraphics2DHost::SendFlushAck() {
  host()->SendReply(flush_reply_context_, PpapiPluginMsg_Graphics2D_FlushAck());
}

void PepperGraphics2DHost::SendOffscreenFlushAck() {
  DCHECK(offscreen_flush_pending_);

  // We must clear this flag before issuing the callback. It will be
  // common for the plugin to issue another invalidate in response to a flush
  // callback, and we don't want to think that a callback is already pending.
  offscreen_flush_pending_ = false;
  SendFlushAck();
}

void PepperGraphics2DHost::ScheduleOffscreenFlushAck() {
  offscreen_flush_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PepperGraphics2DHost::SendOffscreenFlushAck,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kOffscreenCallbackDelayMs));
}

bool PepperGraphics2DHost::HasPendingFlush() const {
  return need_flush_ack_ || offscreen_flush_pending_;
}

// static
bool PepperGraphics2DHost::ConvertToLogicalPixels(float scale,
                                                  gfx::Rect* op_rect,
                                                  gfx::Point* delta) {
  if (scale == 1.0f || scale <= 0.0f)
    return true;

  gfx::Rect original_rect = *op_rect;
  // Take the enclosing rectangle after scaling so a rectangle scaled down then
  // scaled back up by the inverse scale would fully contain the entire area
  // affected by the original rectangle.
  *op_rect = gfx::ScaleToEnclosingRect(*op_rect, scale);
  if (delta) {
    gfx::Point original_delta = *delta;
    float inverse_scale = 1.0f / scale;
    *delta = gfx::ScaleToFlooredPoint(*delta, scale);

    gfx::Rect inverse_scaled_rect =
        gfx::ScaleToEnclosingRect(*op_rect, inverse_scale);
    if (original_rect != inverse_scaled_rect)
      return false;
    gfx::Point inverse_scaled_point =
        gfx::ScaleToFlooredPoint(*delta, inverse_scale);
    if (original_delta != inverse_scaled_point)
      return false;
  }

  return true;
}

}  // namespace content
