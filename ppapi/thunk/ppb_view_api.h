// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_VIEW_API_H_
#define PPAPI_THUNK_PPB_VIEW_API_H_

#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct ViewData;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_View_API {
 public:
  virtual ~PPB_View_API() {}

  // Returns the view data struct.
  virtual const ViewData& GetData() const = 0;

  virtual PP_Bool GetRect(PP_Rect* viewport) const = 0;
  virtual PP_Bool IsFullscreen() const = 0;
  virtual PP_Bool IsVisible() const = 0;
  virtual PP_Bool IsPageVisible() const = 0;
  virtual PP_Bool GetClipRect(PP_Rect* clip) const = 0;
  virtual float GetDeviceScale() const = 0;
  virtual float GetCSSScale() const = 0;
  virtual PP_Bool GetScrollOffset(PP_Point* offset) const = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif // PPAPI_THUNK_PPB_VIEW_API_H_
