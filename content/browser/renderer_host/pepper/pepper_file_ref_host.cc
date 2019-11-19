// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"

#include <string>

#include "content/browser/renderer_host/pepper/pepper_external_file_ref_backend.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/browser/renderer_host/pepper/pepper_internal_file_ref_backend.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_ref_util.h"
#include "storage/browser/file_system/file_permission_policy.h"

using ppapi::host::ResourceHost;

namespace content {

PepperFileRefBackend::~PepperFileRefBackend() {}

PepperFileRefHost::PepperFileRefHost(BrowserPpapiHost* host,
                                     PP_Instance instance,
                                     PP_Resource resource,
                                     PP_Resource file_system,
                                     const std::string& path)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host),
      fs_type_(PP_FILESYSTEMTYPE_INVALID) {
  if (!ppapi::IsValidInternalPath(path))
    return;

  int render_process_id;
  int unused;
  if (!host->GetRenderFrameIDsForInstance(
          instance, &render_process_id, &unused)) {
    return;
  }

  ResourceHost* fs_resource_host =
      host->GetPpapiHost()->GetResourceHost(file_system);
  if (fs_resource_host == nullptr) {
    DLOG(ERROR) << "Couldn't find FileSystem host: " << resource
                << " path: " << path;
    return;
  }

  if (!fs_resource_host->IsFileSystemHost()) {
    DLOG(ERROR) << "Filesystem PP_Resource is not PepperFileSystemBrowserHost";
    return;
  }

  PepperFileSystemBrowserHost* file_system_host =
      static_cast<PepperFileSystemBrowserHost*>(fs_resource_host);
  file_system_host_ = file_system_host->AsWeakPtr();
  fs_type_ = file_system_host->GetType();
  if ((fs_type_ != PP_FILESYSTEMTYPE_LOCALPERSISTENT) &&
      (fs_type_ != PP_FILESYSTEMTYPE_LOCALTEMPORARY) &&
      (fs_type_ != PP_FILESYSTEMTYPE_EXTERNAL) &&
      (fs_type_ != PP_FILESYSTEMTYPE_ISOLATED)) {
    DLOG(ERROR) << "Unsupported filesystem type: " << fs_type_;
    return;
  }
  if ((fs_type_ == PP_FILESYSTEMTYPE_EXTERNAL) &&
      (!file_system_host->GetRootUrl().is_valid())) {
    DLOG(ERROR) << "Native external filesystems are not supported by this "
                << "constructor.";
    return;
  }

  backend_.reset(new PepperInternalFileRefBackend(host->GetPpapiHost(),
                                                  render_process_id,
                                                  file_system_host->AsWeakPtr(),
                                                  path));
}

PepperFileRefHost::PepperFileRefHost(BrowserPpapiHost* host,
                                     PP_Instance instance,
                                     PP_Resource resource,
                                     const base::FilePath& external_path)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host),
      fs_type_(PP_FILESYSTEMTYPE_EXTERNAL) {
  if (!ppapi::IsValidExternalPath(external_path))
    return;

  int render_process_id;
  int unused;
  if (!host->GetRenderFrameIDsForInstance(
          instance, &render_process_id, &unused)) {
    return;
  }

  backend_.reset(new PepperExternalFileRefBackend(
      host->GetPpapiHost(), render_process_id, external_path));
}

PepperFileRefHost::~PepperFileRefHost() {}

bool PepperFileRefHost::IsFileRefHost() { return true; }

PP_FileSystemType PepperFileRefHost::GetFileSystemType() const {
  return fs_type_;
}

storage::FileSystemURL PepperFileRefHost::GetFileSystemURL() const {
  if (backend_)
    return backend_->GetFileSystemURL();
  return storage::FileSystemURL();
}

base::FilePath PepperFileRefHost::GetExternalFilePath() const {
  if (backend_)
    return backend_->GetExternalFilePath();
  return base::FilePath();
}

base::WeakPtr<PepperFileSystemBrowserHost>
PepperFileRefHost::GetFileSystemHost() const {
  return file_system_host_;
}

int32_t PepperFileRefHost::CanRead() const {
  if (backend_)
    return backend_->CanRead();
  return PP_ERROR_FAILED;
}

int32_t PepperFileRefHost::CanWrite() const {
  if (backend_)
    return backend_->CanWrite();
  return PP_ERROR_FAILED;
}

int32_t PepperFileRefHost::CanCreate() const {
  if (backend_)
    return backend_->CanCreate();
  return PP_ERROR_FAILED;
}

int32_t PepperFileRefHost::CanReadWrite() const {
  if (backend_)
    return backend_->CanReadWrite();
  return PP_ERROR_FAILED;
}

int32_t PepperFileRefHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  if (!backend_)
    return PP_ERROR_FAILED;

  PPAPI_BEGIN_MESSAGE_MAP(PepperFileRefHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileRef_MakeDirectory,
                                      OnMakeDirectory)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileRef_Touch, OnTouch)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileRef_Delete, OnDelete)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileRef_Rename, OnRename)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileRef_Query, OnQuery)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_FileRef_ReadDirectoryEntries, OnReadDirectoryEntries)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileRef_GetAbsolutePath,
                                        OnGetAbsolutePath)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFileRefHost::OnMakeDirectory(
    ppapi::host::HostMessageContext* context,
    int32_t make_directory_flags) {
  int32_t rv = CanCreate();
  if (rv != PP_OK)
    return rv;
  return backend_->MakeDirectory(context->MakeReplyMessageContext(),
                                 make_directory_flags);
}

int32_t PepperFileRefHost::OnTouch(ppapi::host::HostMessageContext* context,
                                   PP_Time last_access_time,
                                   PP_Time last_modified_time) {
  // TODO(teravest): Change this to be kWriteFilePermissions here and in
  // fileapi_message_filter.
  int32_t rv = CanCreate();
  if (rv != PP_OK)
    return rv;
  return backend_->Touch(
      context->MakeReplyMessageContext(), last_access_time, last_modified_time);
}

int32_t PepperFileRefHost::OnDelete(ppapi::host::HostMessageContext* context) {
  int32_t rv = CanWrite();
  if (rv != PP_OK)
    return rv;
  return backend_->Delete(context->MakeReplyMessageContext());
}

int32_t PepperFileRefHost::OnRename(ppapi::host::HostMessageContext* context,
                                    PP_Resource new_file_ref) {
  int32_t rv = CanReadWrite();
  if (rv != PP_OK)
    return rv;

  ResourceHost* resource_host =
      host_->GetPpapiHost()->GetResourceHost(new_file_ref);
  if (!resource_host)
    return PP_ERROR_BADRESOURCE;

  PepperFileRefHost* file_ref_host = nullptr;
  if (resource_host->IsFileRefHost())
    file_ref_host = static_cast<PepperFileRefHost*>(resource_host);
  if (!file_ref_host)
    return PP_ERROR_BADRESOURCE;

  rv = file_ref_host->CanCreate();
  if (rv != PP_OK)
    return rv;

  return backend_->Rename(context->MakeReplyMessageContext(), file_ref_host);
}

int32_t PepperFileRefHost::OnQuery(ppapi::host::HostMessageContext* context) {
  int32_t rv = CanRead();
  if (rv != PP_OK)
    return rv;
  return backend_->Query(context->MakeReplyMessageContext());
}

int32_t PepperFileRefHost::OnReadDirectoryEntries(
    ppapi::host::HostMessageContext* context) {
  int32_t rv = CanRead();
  if (rv != PP_OK)
    return rv;
  return backend_->ReadDirectoryEntries(context->MakeReplyMessageContext());
}

int32_t PepperFileRefHost::OnGetAbsolutePath(
    ppapi::host::HostMessageContext* context) {
  if (!host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_PRIVATE))
    return PP_ERROR_NOACCESS;
  return backend_->GetAbsolutePath(context->MakeReplyMessageContext());
}

}  // namespace content
