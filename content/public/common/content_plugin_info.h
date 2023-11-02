// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_PLUGIN_INFO_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_PLUGIN_INFO_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "content/public/common/webplugininfo.h"
#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#endif  // BUILDFLAG(ENABLE_PPAPI)

namespace content {

struct CONTENT_EXPORT ContentPluginInfo {
#if BUILDFLAG(ENABLE_PPAPI)
  typedef const void* (*GetInterfaceFunc)(const char*);
  typedef int (*PPP_InitializeModuleFunc)(PP_Module, PPB_GetInterface);
  typedef void (*PPP_ShutdownModuleFunc)();

  struct EntryPoints {
    // This structure is POD, with the constructor initializing to NULL.
    CONTENT_EXPORT EntryPoints();

    GetInterfaceFunc get_interface = nullptr;
    PPP_InitializeModuleFunc initialize_module = nullptr;
    PPP_ShutdownModuleFunc shutdown_module = nullptr;  // Optional, may be NULL.
  };
#endif  // BUILDFLAG(ENABLE_PPAPI)

  ContentPluginInfo();
  ContentPluginInfo(const ContentPluginInfo& other);
  ContentPluginInfo(ContentPluginInfo&& other) noexcept;
  ~ContentPluginInfo();

  WebPluginInfo ToWebPluginInfo() const;

  // Indicates internal plugins for which there's not actually a library.
  // These plugins are implemented in the Chrome binary using a separate set
  // of entry points (see internal_entry_points below).
  // Defaults to false.
  bool is_internal = false;

  // True when this plugin should be run out of process. Defaults to false.
  bool is_out_of_process = false;

  base::FilePath path;  // Internal plugins have "internal-[name]" as path.
  std::string name;
  std::string description;
  std::string version;
  std::vector<WebPluginMimeType> mime_types;

#if BUILDFLAG(ENABLE_PPAPI)
  // When is_internal is set, this contains the function pointers to the
  // entry points for the internal plugins.
  EntryPoints internal_entry_points;
#endif  // BUILDFLAG(ENABLE_PPAPI)

  // Permission bits from ppapi::Permission.
  uint32_t permissions = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_PLUGIN_INFO_H_
