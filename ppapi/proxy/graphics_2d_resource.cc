// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/graphics_2d_resource.h"

#include "base/functional/bind.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"

namespace ppapi {
namespace proxy {

Graphics2DResource::Graphics2DResource(Connection connection,
                                       PP_Instance instance,
                                       const PP_Size& size,
                                       PP_Bool is_always_opaque)
    : PluginResource(connection, instance),
      size_(size),
      is_always_opaque_(is_always_opaque),
      scale_(1.0f) {
  // These checks are copied from PPB_ImageData_Impl::Init to make tests passed.
  // Let's remove/refactor this when start to refactor ImageData.
  bool bad_args =
      size.width <= 0 || size.height <= 0 ||
      static_cast<int64_t>(size.width) * static_cast<int64_t>(size.height) >=
          std::numeric_limits<int32_t>::max() / 4;
  if (!bad_args && !sent_create_to_renderer()) {
    SendCreate(RENDERER,
        PpapiHostMsg_Graphics2D_Create(size, is_always_opaque));
  }
}

Graphics2DResource::~Graphics2DResource() {
}

PP_Bool Graphics2DResource::Describe(PP_Size* size, PP_Bool* is_always_opaque) {
  *size = size_;
  *is_always_opaque = is_always_opaque_;
  return PP_TRUE;
}

thunk::PPB_Graphics2D_API* Graphics2DResource::AsPPB_Graphics2D_API() {
  return this;
}

void Graphics2DResource::PaintImageData(PP_Resource image_data,
                                        const PP_Point* top_left,
                                        const PP_Rect* src_rect) {
  Resource* image_object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(image_data);
  if (!image_object || pp_instance() != image_object->pp_instance()) {
    Log(PP_LOGLEVEL_ERROR,
        "Graphics2DResource.PaintImageData: Bad image resource.");
    return;
  }

  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  Post(RENDERER, PpapiHostMsg_Graphics2D_PaintImageData(
      image_object->host_resource(), *top_left,
      !!src_rect, src_rect ? *src_rect : dummy));
}

void Graphics2DResource::Scroll(const PP_Rect* clip_rect,
                                const PP_Point* amount) {
  PP_Rect dummy;
  memset(&dummy, 0, sizeof(PP_Rect));
  Post(RENDERER, PpapiHostMsg_Graphics2D_Scroll(
      !!clip_rect, clip_rect ? *clip_rect : dummy, *amount));
}

void Graphics2DResource::ReplaceContents(PP_Resource image_data) {
  thunk::EnterResourceNoLock<thunk::PPB_ImageData_API> enter_image(
      image_data, true);
  if (enter_image.failed())
    return;

  // Check that the PP_Instance matches.
  Resource* image_object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(image_data);
  if (!image_object || pp_instance() != image_object->pp_instance()) {
    Log(PP_LOGLEVEL_ERROR,
        "Graphics2DResource.PaintImageData: Bad image resource.");
    return;
  }
  enter_image.object()->SetIsCandidateForReuse();

  Post(RENDERER, PpapiHostMsg_Graphics2D_ReplaceContents(
      image_object->host_resource()));
}

PP_Bool Graphics2DResource::SetScale(float scale) {
  if (scale <= 0.0f)
    return PP_FALSE;
  Post(RENDERER, PpapiHostMsg_Graphics2D_SetScale(scale));
  scale_ = scale;
  return PP_TRUE;
}

float Graphics2DResource::GetScale() {
  return scale_;
}

PP_Bool Graphics2DResource::SetLayerTransform(float scale,
                                              const PP_Point* origin,
                                              const PP_Point* translate) {
  if (scale <= 0.0f)
    return PP_FALSE;
  // Adding the origin to the transform.
  PP_FloatPoint translate_with_origin;
  translate_with_origin.x = (1 - scale) * origin->x - translate->x;
  translate_with_origin.y = (1 - scale) * origin->y - translate->y;
  Post(RENDERER,
       PpapiHostMsg_Graphics2D_SetLayerTransform(scale, translate_with_origin));
  return PP_TRUE;
}

int32_t Graphics2DResource::Flush(scoped_refptr<TrackedCallback> callback) {
  // If host is not even created, return failure immediately.  This can happen
  // when failed to initialize (in constructor).
  if (!sent_create_to_renderer())
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(current_flush_callback_))
    return PP_ERROR_INPROGRESS;  // Can't have >1 flush pending.
  current_flush_callback_ = callback;

  Call<PpapiPluginMsg_Graphics2D_FlushAck>(
      RENDERER, PpapiHostMsg_Graphics2D_Flush(),
      base::BindOnce(&Graphics2DResource::OnPluginMsgFlushACK, this));
  return PP_OK_COMPLETIONPENDING;
}

bool Graphics2DResource::ReadImageData(PP_Resource image,
                                       const PP_Point* top_left) {
  if (!top_left)
    return false;
  int32_t result = SyncCall<PpapiPluginMsg_Graphics2D_ReadImageDataAck>(
      RENDERER,
      PpapiHostMsg_Graphics2D_ReadImageData(image, *top_left));
  return result == PP_OK;
}

void Graphics2DResource::OnPluginMsgFlushACK(
    const ResourceMessageReplyParams& params) {
  current_flush_callback_->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
