// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_GRAPHICS_2D_RESOURCE_H_
#define PPAPI_PROXY_GRAPHICS_2D_RESOURCE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT Graphics2DResource : public PluginResource,
                                              public thunk::PPB_Graphics2D_API {
 public:
  Graphics2DResource(Connection connection,
                     PP_Instance instance,
                     const PP_Size& size,
                     PP_Bool is_always_opaque);

  Graphics2DResource(const Graphics2DResource&) = delete;
  Graphics2DResource& operator=(const Graphics2DResource&) = delete;

  ~Graphics2DResource() override;

  // Resource overrides.
  thunk::PPB_Graphics2D_API* AsPPB_Graphics2D_API() override;

  // PPB_Graphics2D_API overrides.
  PP_Bool Describe(PP_Size* size, PP_Bool* is_always_opaque) override;
  void PaintImageData(PP_Resource image_data,
                      const PP_Point* top_left,
                      const PP_Rect* src_rect) override;
  void Scroll(const PP_Rect* clip_rect, const PP_Point* amount) override;
  void ReplaceContents(PP_Resource image_data) override;
  PP_Bool SetScale(float scale) override;
  float GetScale() override;
  PP_Bool SetLayerTransform(float scale,
                            const PP_Point* origin,
                            const PP_Point* translate) override;
  int32_t Flush(scoped_refptr<TrackedCallback> callback) override;
  bool ReadImageData(PP_Resource image, const PP_Point* top_left) override;

 private:
  void OnPluginMsgFlushACK(const ResourceMessageReplyParams& params);

  const PP_Size size_;
  const PP_Bool is_always_opaque_;
  float scale_;

  scoped_refptr<TrackedCallback> current_flush_callback_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_GRAPHICS_2D_RESOURCE_H_
