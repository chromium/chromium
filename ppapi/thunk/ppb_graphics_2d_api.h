// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_GRAPHICS_2D_API_H_
#define PPAPI_THUNK_PPB_GRAPHICS_2D_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Graphics2D_API {
 public:
  virtual ~PPB_Graphics2D_API() {}

  virtual PP_Bool Describe(PP_Size* size, PP_Bool* is_always_opaque) = 0;
  virtual void PaintImageData(PP_Resource image_data,
                              const PP_Point* top_left,
                              const PP_Rect* src_rect) = 0;
  virtual void Scroll(const PP_Rect* clip_rect,
                      const PP_Point* amount) = 0;
  virtual void ReplaceContents(PP_Resource image_data) = 0;
  virtual int32_t Flush(scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Bool SetScale(float scale) = 0;
  virtual float GetScale() = 0;
  virtual PP_Bool SetLayerTransform(float scale,
                                    const PP_Point* origin,
                                    const PP_Point* translate) = 0;

  // Test only
  virtual bool ReadImageData(PP_Resource image, const PP_Point* top_left) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_GRAPHICS_2D_API_H_
