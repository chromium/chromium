// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_REF_CREATE_INFO_H_
#define PPAPI_SHARED_IMPL_FILE_REF_CREATE_INFO_H_

#include <string>

#include "base/files/file_path.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// FileRefs are created in a number of places and they include a number of
// return values. This struct encapsulates everything in one place.
struct FileRefCreateInfo {
  FileRefCreateInfo()
      : file_system_type(PP_FILESYSTEMTYPE_INVALID),
        browser_pending_host_resource_id(0),
        renderer_pending_host_resource_id(0),
        file_system_plugin_resource(0) {}

  PPAPI_SHARED_EXPORT bool IsValid() const;

  PP_FileSystemType file_system_type;
  std::string internal_path;
  std::string display_name;

  // Used when a FileRef is created in the Renderer.
  int browser_pending_host_resource_id;
  int renderer_pending_host_resource_id;

  // Since FileRef needs to hold a FileSystem reference, we need to pass the
  // resource in this CreateInfo. This struct doesn't hold any refrence on the
  // file_system_plugin_resource.
  PP_Resource file_system_plugin_resource;
};

// Used in the renderer when sending a FileRefCreateInfo to a plugin for a
// FileRef on an external filesystem.
PPAPI_SHARED_EXPORT FileRefCreateInfo
    MakeExternalFileRefCreateInfo(const base::FilePath& external_path,
                                  const std::string& display_name,
                                  int browser_pending_host_resource_id,
                                  int renderer_pending_host_resource_id);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_REF_CREATE_INFO_H_
