// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/isolated_file_system_private_resource.h"

#include "base/functional/bind.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/proxy/file_system_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace proxy {

namespace {
void RunTrackedCallback(scoped_refptr<TrackedCallback> callback,
                        int32_t rc) {
  callback->Run(rc);
}
}  // namespace

IsolatedFileSystemPrivateResource::IsolatedFileSystemPrivateResource(
    Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(BROWSER, PpapiHostMsg_IsolatedFileSystem_Create());
}

IsolatedFileSystemPrivateResource::~IsolatedFileSystemPrivateResource() {
}

thunk::PPB_IsolatedFileSystem_Private_API*
IsolatedFileSystemPrivateResource::AsPPB_IsolatedFileSystem_Private_API() {
  return this;
}

int32_t IsolatedFileSystemPrivateResource::Open(
    PP_Instance /* unused */,
    PP_IsolatedFileSystemType_Private type,
    PP_Resource* file_system_resource,
    scoped_refptr<TrackedCallback> callback) {
  if (!file_system_resource)
    return PP_ERROR_BADARGUMENT;

  Call<PpapiPluginMsg_IsolatedFileSystem_BrowserOpenReply>(
      BROWSER, PpapiHostMsg_IsolatedFileSystem_BrowserOpen(type),
      base::BindOnce(&IsolatedFileSystemPrivateResource::OnBrowserOpenComplete,
                     this, type, file_system_resource, callback));
  return PP_OK_COMPLETIONPENDING;
}

void IsolatedFileSystemPrivateResource::OnBrowserOpenComplete(
    PP_IsolatedFileSystemType_Private type,
    PP_Resource* file_system_resource,
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    const std::string& fsid) {
  if (!TrackedCallback::IsPending(callback))
    return;

  if (params.result() != PP_OK) {
    callback->Run(params.result());
    return;
  }

  FileSystemResource* fs = new FileSystemResource(
      connection(), pp_instance(), PP_FILESYSTEMTYPE_ISOLATED);
  *file_system_resource = fs->GetReference();
  if (*file_system_resource == 0)
    callback->Run(PP_ERROR_FAILED);
  fs->InitIsolatedFileSystem(fsid, type,
                             base::BindOnce(&RunTrackedCallback, callback));
}

}  // namespace proxy
}  // namespace ppapi
