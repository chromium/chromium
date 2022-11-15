// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CRX filesystem is a filesystem that allows an extension to read its own
// package directory tree.  See ppapi/examples/crxfs for example.
//
// IMPLEMENTATION
//
// The implementation involves both browser and renderer.  In order to provide
// readonly access to CRX filesystem (i.e. extension directory), we create an
// "isolated filesystem" pointing to current extension directory in browser.
// Then browser grants read permission to renderer, and tells plugin the
// filesystem id, or fsid.
//
// Once the plugin receives the fsid, it creates a PPB_FileSystem and forwards
// the fsid to PepperFileSystemHost in order to construct root url.

#ifndef PPAPI_PROXY_ISOLATED_FILE_SYSTEM_PRIVATE_RESOURCE_H_
#define PPAPI_PROXY_ISOLATED_FILE_SYSTEM_PRIVATE_RESOURCE_H_

#include <stdint.h>

#include <string>

#include "base/memory/scoped_refptr.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_isolated_file_system_private_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class ResourceMessageReplyParams;

class PPAPI_PROXY_EXPORT IsolatedFileSystemPrivateResource
    : public PluginResource,
      public thunk::PPB_IsolatedFileSystem_Private_API {
 public:
  IsolatedFileSystemPrivateResource(
      Connection connection, PP_Instance instance);

  IsolatedFileSystemPrivateResource(const IsolatedFileSystemPrivateResource&) =
      delete;
  IsolatedFileSystemPrivateResource& operator=(
      const IsolatedFileSystemPrivateResource&) = delete;

  ~IsolatedFileSystemPrivateResource() override;

  // Resource overrides.
  thunk::PPB_IsolatedFileSystem_Private_API*
      AsPPB_IsolatedFileSystem_Private_API() override;

  // PPB_IsolatedFileSystem_Private_API implementation.
  int32_t Open(PP_Instance instance,
               PP_IsolatedFileSystemType_Private type,
               PP_Resource* file_system_resource,
               scoped_refptr<TrackedCallback> callback) override;

 private:
  void OnBrowserOpenComplete(PP_IsolatedFileSystemType_Private type,
                             PP_Resource* file_system_resource,
                             scoped_refptr<TrackedCallback> callback,
                             const ResourceMessageReplyParams& params,
                             const std::string& fsid);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_ISOLATED_FILE_SYSTEM_PRIVATE_RESOURCE_H_
