// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_internal_file_ref_backend.h"

#include <string.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/escape.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/file_ref_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "ppapi/thunk/ppb_file_system_api.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"

using ppapi::host::PpapiHost;
using ppapi::host::ResourceHost;

namespace content {

PepperInternalFileRefBackend::PepperInternalFileRefBackend(
    PpapiHost* host,
    int render_process_id,
    base::WeakPtr<PepperFileSystemBrowserHost> fs_host,
    const std::string& path)
    : host_(host),
      render_process_id_(render_process_id),
      fs_host_(fs_host),
      fs_type_(fs_host->GetType()),
      path_(path) {
  ppapi::NormalizeInternalPath(&path_);
}

PepperInternalFileRefBackend::~PepperInternalFileRefBackend() {}

storage::FileSystemURL PepperInternalFileRefBackend::GetFileSystemURL() const {
  if (!fs_url_.is_valid() && fs_host_.get() && fs_host_->IsOpened()) {
    GURL fs_path =
        fs_host_->GetRootUrl().Resolve(net::EscapePath(path_.substr(1)));
    scoped_refptr<storage::FileSystemContext> fs_context =
        GetFileSystemContext();
    if (fs_context.get())
      fs_url_ = fs_context->CrackURL(fs_path);
  }
  return fs_url_;
}

base::FilePath PepperInternalFileRefBackend::GetExternalFilePath() const {
  return base::FilePath();
}

scoped_refptr<storage::FileSystemContext>
PepperInternalFileRefBackend::GetFileSystemContext() const {
  if (!fs_host_.get())
    return nullptr;
  return fs_host_->GetFileSystemContext();
}

void PepperInternalFileRefBackend::DidFinish(
    ppapi::host::ReplyMessageContext context,
    const IPC::Message& msg,
    base::File::Error error) {
  context.params.set_result(ppapi::FileErrorToPepperError(error));
  host_->SendReply(context, msg);
}

int32_t PepperInternalFileRefBackend::MakeDirectory(
    ppapi::host::ReplyMessageContext reply_context,
    int32_t make_directory_flags) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  GetFileSystemContext()->operation_runner()->CreateDirectory(
      GetFileSystemURL(),
      !!(make_directory_flags & PP_MAKEDIRECTORYFLAG_EXCLUSIVE),
      !!(make_directory_flags & PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS),
      base::BindOnce(&PepperInternalFileRefBackend::DidFinish,
                     weak_factory_.GetWeakPtr(), reply_context,
                     PpapiPluginMsg_FileRef_MakeDirectoryReply()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Touch(
    ppapi::host::ReplyMessageContext reply_context,
    PP_Time last_access_time,
    PP_Time last_modified_time) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  GetFileSystemContext()->operation_runner()->TouchFile(
      GetFileSystemURL(), ppapi::PPTimeToTime(last_access_time),
      ppapi::PPTimeToTime(last_modified_time),
      base::BindOnce(&PepperInternalFileRefBackend::DidFinish,
                     weak_factory_.GetWeakPtr(), reply_context,
                     PpapiPluginMsg_FileRef_TouchReply()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Delete(
    ppapi::host::ReplyMessageContext reply_context) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  GetFileSystemContext()->operation_runner()->Remove(
      GetFileSystemURL(), false,
      base::BindOnce(&PepperInternalFileRefBackend::DidFinish,
                     weak_factory_.GetWeakPtr(), reply_context,
                     PpapiPluginMsg_FileRef_DeleteReply()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Rename(
    ppapi::host::ReplyMessageContext reply_context,
    PepperFileRefHost* new_file_ref) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  storage::FileSystemURL new_url = new_file_ref->GetFileSystemURL();
  if (!new_url.is_valid())
    return PP_ERROR_FAILED;
  if (!new_url.IsInSameFileSystem(GetFileSystemURL()))
    return PP_ERROR_FAILED;

  GetFileSystemContext()->operation_runner()->Move(
      GetFileSystemURL(), new_url, storage::FileSystemOperation::OPTION_NONE,
      base::BindOnce(&PepperInternalFileRefBackend::DidFinish,
                     weak_factory_.GetWeakPtr(), reply_context,
                     PpapiPluginMsg_FileRef_RenameReply()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Query(
    ppapi::host::ReplyMessageContext reply_context) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  GetFileSystemContext()->operation_runner()->GetMetadata(
      GetFileSystemURL(),
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
          storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&PepperInternalFileRefBackend::GetMetadataComplete,
                     weak_factory_.GetWeakPtr(), reply_context));
  return PP_OK_COMPLETIONPENDING;
}

void PepperInternalFileRefBackend::GetMetadataComplete(
    ppapi::host::ReplyMessageContext reply_context,
    base::File::Error error,
    const base::File::Info& file_info) {
  reply_context.params.set_result(ppapi::FileErrorToPepperError(error));

  PP_FileInfo pp_file_info;
  if (error == base::File::FILE_OK)
    ppapi::FileInfoToPepperFileInfo(file_info, fs_type_, &pp_file_info);
  else
    memset(&pp_file_info, 0, sizeof(pp_file_info));

  host_->SendReply(reply_context,
                   PpapiPluginMsg_FileRef_QueryReply(pp_file_info));
}

int32_t PepperInternalFileRefBackend::ReadDirectoryEntries(
    ppapi::host::ReplyMessageContext reply_context) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  storage::FileSystemOperation::FileEntryList* accumulated_file_list =
      new storage::FileSystemOperation::FileEntryList;
  GetFileSystemContext()->operation_runner()->ReadDirectory(
      GetFileSystemURL(),
      base::BindRepeating(&PepperInternalFileRefBackend::ReadDirectoryComplete,
                          weak_factory_.GetWeakPtr(), reply_context,
                          base::Owned(accumulated_file_list)));
  return PP_OK_COMPLETIONPENDING;
}

void PepperInternalFileRefBackend::ReadDirectoryComplete(
    ppapi::host::ReplyMessageContext context,
    storage::FileSystemOperation::FileEntryList* accumulated_file_list,
    base::File::Error error,
    storage::FileSystemOperation::FileEntryList file_list,
    bool has_more) {
  accumulated_file_list->insert(
      accumulated_file_list->end(), file_list.begin(), file_list.end());
  if (has_more)
    return;

  context.params.set_result(ppapi::FileErrorToPepperError(error));

  std::vector<ppapi::FileRefCreateInfo> infos;
  std::vector<PP_FileType> file_types;
  if (error == base::File::FILE_OK && fs_host_.get()) {
    std::string dir_path = path_;
    if (dir_path.empty() || dir_path.back() != '/')
      dir_path += '/';

    for (const auto& it : *accumulated_file_list) {
      file_types.push_back(it.type == filesystem::mojom::FsFileType::DIRECTORY
                               ? PP_FILETYPE_DIRECTORY
                               : PP_FILETYPE_REGULAR);

      ppapi::FileRefCreateInfo info;
      info.file_system_type = fs_type_;
      info.file_system_plugin_resource = fs_host_->pp_resource();
      std::string path =
          dir_path + storage::FilePathToString(base::FilePath(it.name));
      info.internal_path = path;
      info.display_name = ppapi::GetNameForInternalFilePath(path);
      infos.push_back(info);
    }
  }

  host_->SendReply(
      context,
      PpapiPluginMsg_FileRef_ReadDirectoryEntriesReply(infos, file_types));
}

int32_t PepperInternalFileRefBackend::GetAbsolutePath(
    ppapi::host::ReplyMessageContext reply_context) {
  host_->SendReply(reply_context,
                   PpapiPluginMsg_FileRef_GetAbsolutePathReply(path_));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::CanRead() const {
  storage::FileSystemURL url = GetFileSystemURL();
  if (!FileSystemURLIsValid(GetFileSystemContext().get(), url))
    return PP_ERROR_FAILED;
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFileSystemFile(
          render_process_id_, url)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

int32_t PepperInternalFileRefBackend::CanWrite() const {
  storage::FileSystemURL url = GetFileSystemURL();
  if (!FileSystemURLIsValid(GetFileSystemContext().get(), url))
    return PP_ERROR_FAILED;
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanWriteFileSystemFile(
          render_process_id_, url)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

int32_t PepperInternalFileRefBackend::CanCreate() const {
  storage::FileSystemURL url = GetFileSystemURL();
  if (!FileSystemURLIsValid(GetFileSystemContext().get(), url))
    return PP_ERROR_FAILED;
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanCreateFileSystemFile(
          render_process_id_, url)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

int32_t PepperInternalFileRefBackend::CanReadWrite() const {
  storage::FileSystemURL url = GetFileSystemURL();
  if (!FileSystemURLIsValid(GetFileSystemContext().get(), url))
    return PP_ERROR_FAILED;
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadFileSystemFile(render_process_id_, url) ||
      !policy->CanWriteFileSystemFile(render_process_id_, url)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

}  // namespace content
