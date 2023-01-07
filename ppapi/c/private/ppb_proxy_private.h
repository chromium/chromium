// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_PROXY_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_PROXY_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

#define PPB_PROXY_PRIVATE_INTERFACE "PPB_Proxy_Private;6"

// Exposes functions needed by the out-of-process proxy to call into the
// renderer PPAPI implementation.
struct PPB_Proxy_Private {
  // Called when the given plugin process has crashed.
  void (*PluginCrashed)(PP_Module module);

  // Returns the instance for the given resource, or 0 on failure.
  PP_Instance (*GetInstanceForResource)(PP_Resource resource);

  // Sets a callback that will be used to make sure that PP_Instance IDs
  // are unique in the plugin.
  //
  // Since the plugin may be shared between several browser processes, we need
  // to do extra work to make sure that an instance ID is globally unqiue. The
  // given function will be called and will return true if the given
  // PP_Instance is OK to use in the plugin. It will then be marked as "in use"
  // On failure (returns false), the host implementation will generate a new
  // instance ID and try again.
  void (*SetReserveInstanceIDCallback)(
      PP_Module module,
      PP_Bool (*is_seen)(PP_Module, PP_Instance));

  // Allows adding additional refcounts to the PluginModule that owns the
  // proxy dispatcher (and all interface proxies). For every AddRef call
  // there must be a corresponding release call.
  void (*AddRefModule)(PP_Module module);
  void (*ReleaseModule)(PP_Module module);

  // Allows asserts to be written for some bad conditions while cleaning up.
  PP_Bool (*IsInModuleDestructor)(PP_Module module);
};

#endif  // PPAPI_C_PRIVATE_PPB_PROXY_PRIVATE_H_
