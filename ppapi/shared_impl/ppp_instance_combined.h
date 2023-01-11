// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_
#define PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// This exposes the 1.1 interface and forwards it to the 1.0 interface is
// necessary.
struct PPAPI_SHARED_EXPORT PPP_Instance_Combined {
 public:
  // Create a PPP_Instance_Combined. Uses the given |get_interface_func| to
  // query the plugin and find the most recent version of the PPP_Instance
  // interface. If the plugin doesn't support any PPP_Instance interface,
  // returns NULL.
  static PPP_Instance_Combined* Create(
      base::RepeatingCallback<const void*(const char*)> get_plugin_if);

  PPP_Instance_Combined(const PPP_Instance_Combined&) = delete;
  PPP_Instance_Combined& operator=(const PPP_Instance_Combined&) = delete;

  PP_Bool DidCreate(PP_Instance instance,
                    uint32_t argc,
                    const char* argn[],
                    const char* argv[]);
  void DidDestroy(PP_Instance instance);

  // This version of DidChangeView encapsulates all arguments for both 1.0
  // and 1.1 versions of this function. Conversion from 1.1 -> 1.0 is easy,
  // but this class doesn't have the necessary context (resource interfaces)
  // to do the conversion, so the caller must do it.
  void DidChangeView(PP_Instance instance,
                     PP_Resource view_changed_resource,
                     const struct PP_Rect* position,
                     const struct PP_Rect* clip);

  void DidChangeFocus(PP_Instance instance, PP_Bool has_focus);
  PP_Bool HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader);

 private:
  explicit PPP_Instance_Combined(const PPP_Instance_1_0& instance_if);
  explicit PPP_Instance_Combined(const PPP_Instance_1_1& instance_if);

  // For version 1.0, DidChangeView will be NULL, and DidChangeView_1_0 will
  // be set below.
  PPP_Instance_1_1 instance_1_1_;

  // Non-NULL when Instance 1.0 is used.
  void (*did_change_view_1_0_)(PP_Instance instance,
                               const struct PP_Rect* position,
                               const struct PP_Rect* clip);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPP_INSTANCE_COMBINED_H_
