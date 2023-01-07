// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_INSTANCE_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_INSTANCE_SHARED_H_

#include "base/compiler_specific.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_Instance_Shared : public thunk::PPB_Instance_API {
 public:
  ~PPB_Instance_Shared() override;

  // Implementation of some shared PPB_Instance_FunctionAPI functions.
  void Log(PP_Instance instance, PP_LogLevel log_level, PP_Var value) override;
  void LogWithSource(PP_Instance instance,
                     PP_LogLevel log_level,
                     PP_Var source,
                     PP_Var value) override;

  // Error checks the given resquest to Request[Filtering]InputEvents. Returns
  // PP_OK if the given classes are all valid, PP_ERROR_NOTSUPPORTED if not.
  int32_t ValidateRequestInputEvents(bool is_filtering, uint32_t event_classes);

  bool ValidateSetCursorParams(PP_MouseCursor_Type type,
                               PP_Resource image,
                               const PP_Point* hot_spot);

  // The length of text to request as a surrounding context of selection.
  // For now, the value is copied from the one with render_view_impl.cc.
  // TODO(kinaba) implement a way to dynamically sync the requirement.
  static const int kExtraCharsForTextInput;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_INSTANCE_SHARED_H_
