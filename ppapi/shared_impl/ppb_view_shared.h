// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_VIEW_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_VIEW_SHARED_H_

#include "base/compiler_specific.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_view_api.h"

namespace ppapi {

// If you add to this struct, be sure to update the serialization in
// ppapi_messages.h.
struct PPAPI_SHARED_EXPORT ViewData {
  ViewData();
  ~ViewData();

  bool Equals(const ViewData& other) const;

  PP_Rect rect;
  bool is_fullscreen;
  bool is_page_visible;
  PP_Rect clip_rect;
  float device_scale;
  float css_scale;
  PP_Point scroll_offset;
};

class PPAPI_SHARED_EXPORT PPB_View_Shared : public Resource,
                                            public thunk::PPB_View_API {
 public:
  PPB_View_Shared(ResourceObjectType type,
                  PP_Instance instance,
                  const ViewData& data);

  PPB_View_Shared(const PPB_View_Shared&) = delete;
  PPB_View_Shared& operator=(const PPB_View_Shared&) = delete;

  ~PPB_View_Shared() override;

  // Resource overrides.
  thunk::PPB_View_API* AsPPB_View_API() override;

  // PPB_View_API implementation.
  const ViewData& GetData() const override;
  PP_Bool GetRect(PP_Rect* viewport) const override;
  PP_Bool IsFullscreen() const override;
  PP_Bool IsVisible() const override;
  PP_Bool IsPageVisible() const override;
  PP_Bool GetClipRect(PP_Rect* clip) const override;
  float GetDeviceScale() const override;
  float GetCSSScale() const override;
  PP_Bool GetScrollOffset(PP_Point* scroll_offset) const override;

 private:
  ViewData data_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_VIEW_SHARED_H_
