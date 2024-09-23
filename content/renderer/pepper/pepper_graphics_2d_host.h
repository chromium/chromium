// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_GRAPHICS_2D_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_GRAPHICS_2D_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/paint/paint_canvas.h"
#include "cc/resources/shared_bitmap_id_registrar.h"
#include "components/viz/common/resources/release_callback.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class CrossThreadSharedBitmap;
}

namespace gfx {
class Rect;
}

namespace gpu {
class ClientSharedImage;
class SharedImageInterface;
}

namespace viz {
class RasterContextProvider;
struct TransferableResource;
}

namespace content {

class PepperPluginInstanceImpl;
class PPB_ImageData_Impl;
class RendererPpapiHost;

class CONTENT_EXPORT PepperGraphics2DHost final
    : public ppapi::host::ResourceHost {
 public:
  static PepperGraphics2DHost* Create(
      RendererPpapiHost* host,
      PP_Instance instance,
      PP_Resource resource,
      const PP_Size& size,
      PP_Bool is_always_opaque,
      scoped_refptr<PPB_ImageData_Impl> backing_store);

  PepperGraphics2DHost(const PepperGraphics2DHost&) = delete;
  PepperGraphics2DHost& operator=(const PepperGraphics2DHost&) = delete;

  ~PepperGraphics2DHost() override;

  // ppapi::host::ResourceHost override.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;
  bool IsGraphics2DHost() override;

  bool ReadImageData(PP_Resource image, const PP_Point* top_left);
  // Assciates this device with the given plugin instance. You can pass NULL
  // to clear the existing device. Returns true on success. In this case, a
  // repaint of the page will also be scheduled. Failure means that the device
  // is already bound to a different instance, and nothing will happen.
  bool BindToInstance(PepperPluginInstanceImpl* new_instance);
  // Paints the current backing store to the web page.
  void Paint(cc::PaintCanvas* canvas,
             const gfx::Rect& plugin_rect,
             const gfx::Rect& paint_rect);

  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* transferable_resource,
      viz::ReleaseCallback* release_callback);
  void AttachedToNewLayer();

  // Notifications about the view's progress painting.  See PluginInstance.
  // These messages are used to send Flush callbacks to the plugin.
  void ViewInitiatedPaint();

  float GetScale() const;
  void SetLayerTransform(float scale, const PP_Point& transform);
  bool IsAlwaysOpaque() const;
  gfx::Size Size() const;

  void ClearCache();

  void set_viewport_to_dip_scale(float viewport_to_dip_scale) {
    DCHECK_LT(0, viewport_to_dip_scale_);
    viewport_to_dip_scale_ = viewport_to_dip_scale;
  }

 private:
  PepperGraphics2DHost(RendererPpapiHost* host,
                       PP_Instance instance,
                       PP_Resource resource);

  bool Init(int width,
            int height,
            bool is_always_opaque,
            scoped_refptr<PPB_ImageData_Impl> backing_store);

  int32_t OnHostMsgPaintImageData(ppapi::host::HostMessageContext* context,
                                  const ppapi::HostResource& image_data,
                                  const PP_Point& top_left,
                                  bool src_rect_specified,
                                  const PP_Rect& src_rect);
  int32_t OnHostMsgScroll(ppapi::host::HostMessageContext* context,
                          bool clip_specified,
                          const PP_Rect& clip,
                          const PP_Point& amount);
  int32_t OnHostMsgReplaceContents(ppapi::host::HostMessageContext* context,
                                   const ppapi::HostResource& image_data);
  int32_t OnHostMsgFlush(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSetScale(ppapi::host::HostMessageContext* context,
                            float scale);
  int32_t OnHostMsgSetLayerTransform(ppapi::host::HostMessageContext* context,
                                     float Scale,
                                     const PP_FloatPoint& Transform);
  int32_t OnHostMsgReadImageData(ppapi::host::HostMessageContext* context,
                                 PP_Resource image,
                                 const PP_Point& top_left);

  // If |old_image_data| is not NULL, a previous used ImageData object will be
  // reused.  This is used by ReplaceContents.
  int32_t Flush(PP_Resource* old_image_data);

  // Called internally to execute the different queued commands. The
  // parameters to these functions will have already been validated. The last
  // rect argument will be filled by each function with the area affected by
  // the update that requires invalidation. If there were no pixels changed,
  // this rect can be untouched.
  void ExecuteTransform(const float& scale,
                        const gfx::PointF& translate,
                        gfx::Rect* invalidated_rect);
  void ExecutePaintImageData(PPB_ImageData_Impl* image,
                             int x,
                             int y,
                             const gfx::Rect& src_rect,
                             gfx::Rect* invalidated_rect);
  void ExecuteScroll(const gfx::Rect& clip,
                     int dx,
                     int dy,
                     gfx::Rect* invalidated_rect);
  void ExecuteReplaceContents(PPB_ImageData_Impl* image,
                              gfx::Rect* invalidated_rect,
                              PP_Resource* old_image_data);

  void SendFlushAck();

  // Function scheduled to execute by ScheduleOffscreenFlushAck that actually
  // issues the offscreen callbacks.
  void SendOffscreenFlushAck();

  // Schedules the offscreen flush ACK at a future time.
  void ScheduleOffscreenFlushAck();

  // Returns true if there is any type of flush callback pending.
  bool HasPendingFlush() const;

  // Scale |op_rect| to logical pixels, taking care to include partially-
  // covered logical pixels (aka DIPs). Also scale optional |delta| to logical
  // pixels as well for scrolling cases. Returns false for scrolling cases where
  // scaling either |op_rect| or |delta| would require scrolling to fall back to
  // invalidation due to rounding errors, true otherwise.
  static bool ConvertToLogicalPixels(float scale,
                                     gfx::Rect* op_rect,
                                     gfx::Point* delta);

  // Callback when compositor is done with a software resource given to it.
  void ReleaseSoftwareCallback(
      scoped_refptr<cc::CrossThreadSharedBitmap> bitmap,
      cc::SharedBitmapIdRegistration registration,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      scoped_refptr<gpu::SharedImageInterface> shared_image_interface,
      const gpu::SyncToken& sync_token,
      bool lost_resource);
  // Callback when compositor is done with a gpu resource given to it. Static
  // for speed. Just kidding, it's so this can clean up the texture if the host
  // has been destroyed.
  static void ReleaseTextureCallback(
      base::WeakPtr<PepperGraphics2DHost> host,
      scoped_refptr<viz::RasterContextProvider> context,
      const gfx::Size& size,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token,
      bool lost);

  raw_ptr<RendererPpapiHost> renderer_ppapi_host_;

  scoped_refptr<PPB_ImageData_Impl> image_data_;

  // Non-owning pointer to the plugin instance this context is currently bound
  // to, if any. If the context is currently unbound, this will be NULL.
  raw_ptr<PepperPluginInstanceImpl> bound_instance_;

  // Keeps track of all drawing commands queued before a Flush call.
  struct QueuedOperation;
  typedef std::vector<QueuedOperation> OperationQueue;
  OperationQueue queued_operations_;

  // True if we need to send an ACK to plugin.
  bool need_flush_ack_;

  // When doing offscreen flushes, we issue a task that issues the callback
  // later. This is set when one of those tasks is pending so that we can
  // enforce the "only one pending flush at a time" constraint in the API.
  bool offscreen_flush_pending_;

  // Set to true if the plugin declares that this device will always be opaque.
  // This allows us to do more optimized painting in some cases.
  bool is_always_opaque_;

  // Set to the scale between what the plugin considers to be one pixel and one
  // DIP
  float scale_;

  // The scale between the viewport and dip.
  float viewport_to_dip_scale_ = 1.0f;

  ppapi::host::ReplyMessageContext flush_reply_context_;

  bool is_running_in_process_;

  bool composited_output_modified_ = true;

  // Local cache of the compositing mode. This is sticky, once true it stays
  // that way.
  bool is_gpu_compositing_disabled_ = false;
  // The shared main thread context provider, used to upload 2d pepper frames
  // if the compositor is expecting gpu content.
  scoped_refptr<viz::RasterContextProvider> main_thread_context_;
  struct SharedImageInfo {
    SharedImageInfo(gpu::SyncToken sync_token,
                    scoped_refptr<gpu::ClientSharedImage> shared_image,
                    gfx::Size size);
    SharedImageInfo(const SharedImageInfo& shared_image_info);
    ~SharedImageInfo();
    gpu::SyncToken sync_token;
    scoped_refptr<gpu::ClientSharedImage> shared_image;
    gfx::Size size;
  };
  // Shared images that are available for recycling.
  std::vector<SharedImageInfo> recycled_shared_images_;

  // This is a bitmap that was recently released by the compositor and may be
  // used to transfer bytes to the compositor again, along with the registration
  // of the SharedBitmapId that is kept alive as long as the bitmap is, in order
  // to give the bitmap to the compositor.
  scoped_refptr<cc::CrossThreadSharedBitmap> cached_bitmap_;
  cc::SharedBitmapIdRegistration cached_bitmap_registration_;
  scoped_refptr<gpu::ClientSharedImage> cached_bitmap_shared_image_;
  // Used for tracking whether the shared_image_interface has changed due to
  // context lost.
  scoped_refptr<gpu::SharedImageInterface>
      cached_bitmap_shared_image_interface_;

  // Whether to use gpu memory for compositor resources.
  const bool enable_gpu_memory_buffer_;

  base::WeakPtrFactory<PepperGraphics2DHost> weak_ptr_factory_{this};

  friend class PepperGraphics2DHostTest;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_GRAPHICS_2D_HOST_H_
