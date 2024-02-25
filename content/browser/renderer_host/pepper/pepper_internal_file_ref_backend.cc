// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_internal_file_ref_backend.h"

#include <string.h>

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
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
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ppapi::host::PpapiHost;
using ppapi::host::ResourceHost;

namespace content {

namespace {

void CallCreateDirectory(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->CreateDirectory(
      url, exclusive, recursive, std::move(callback));
}

void CallReadDirectory(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemOperationRunner::ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->ReadDirectory(url,
                                                         std::move(callback));
}

void CallRemove(scoped_refptr<storage::FileSystemContext> file_system_context,
                const storage::FileSystemURL& url,
                bool recursive,
                storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->Remove(url, recursive,
                                                  std::move(callback));
}

void CallTouchFile(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->TouchFile(
      url, last_access_time, last_modified_time, std::move(callback));
}

void CallMove(scoped_refptr<storage::FileSystemContext> file_system_context,
              const storage::FileSystemURL& src_path,
              const storage::FileSystemURL& dest_path,
              storage::FileSystemOperationRunner::CopyOrMoveOptionSet options,
              storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->Move(
      src_path, dest_path, options,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      std::make_unique<storage::CopyOrMoveHookDelegate>(), std::move(callback));
}

void CallGetMetadata(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    storage::FileSystemOperation::GetMetadataFieldSet fields,
    storage::FileSystemOperationRunner::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->GetMetadata(url, fields,
                                                       std::move(callback));
}

}  // namespace

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
        fs_host_->GetRootUrl().Resolve(base::EscapePath(path_.substr(1)));
    scoped_refptr<storage::FileSystemContext> fs_context =
        GetFileSystemContext();
    if (fs_context.get())
      fs_url_ = fs_context->CrackURL(
          fs_path,
          blink::StorageKey::CreateFirstParty(url::Origin::Create(fs_path)));
  }
  return fs_url_;
}

base::FilePath PepperInternalFileRefBackend::GetExternalFilePath() const {
  return base::FilePath();
}

scoped_refptr<storage::FileSystemContext>
PepperInternalFileRefBackend::GetFileSystemContext() const {
  return PepperFileSystemBrowserHost::GetFileSystemContextFromRenderId(
      render_process_id_);
}

void PepperInternalFileRefBackend::DidFinish(
    ppapi::host::ReplyMessageContext context,
    const IPC::Message& msg,
    base::File::Error error) {
  context.params.set_result(ppapi::FileErrorToPepperError(error));
  host_->SendReply(context, msg);
}

void PepperInternalFileRefBackend::DidFinishOnIOThread(
    base::WeakPtr<PepperInternalFileRefBackend> weak_ptr,
    ppapi::host::ReplyMessageContext reply_context,
    const IPC::Message& msg,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PepperInternalFileRefBackend::DidFinish,
                                weak_ptr, reply_context, msg, error));
}

void PepperInternalFileRefBackend::ReadDirectoryCompleteOnIOThread(
    base::WeakPtr<PepperInternalFileRefBackend> weak_ptr,
    ppapi::host::ReplyMessageContext reply_context,
    storage::FileSystemOperation::FileEntryList* accumulated_file_list,
    base::File::Error error,
    storage::FileSystemOperation::FileEntryList file_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Don't insert the last |file_list| since it'll be added in the post task
  // below.
  if (has_more) {
    accumulated_file_list->insert(accumulated_file_list->end(),
                                  file_list.begin(), file_list.end());
    return;
  }

  // If there are no more, this callback will be deleted and with it
  // |accumulated_file_list| so make a new copy.
  auto* accumulated_file_list2 =
      new storage::FileSystemOperation::FileEntryList;
  accumulated_file_list2->swap(*accumulated_file_list);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperInternalFileRefBackend::ReadDirectoryComplete,
                     weak_ptr, reply_context,
                     base::Owned(accumulated_file_list2), error, file_list,
                     false));
}

void PepperInternalFileRefBackend::GetMetadataCompleteOnIOThread(
    base::WeakPtr<PepperInternalFileRefBackend> weak_ptr,
    ppapi::host::ReplyMessageContext reply_context,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperInternalFileRefBackend::GetMetadataComplete,
                     weak_ptr, reply_context, result, file_info));
}

int32_t PepperInternalFileRefBackend::MakeDirectory(
    ppapi::host::ReplyMessageContext reply_context,
    int32_t make_directory_flags) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  bool exclusive = !!(make_directory_flags & PP_MAKEDIRECTORYFLAG_EXCLUSIVE);
  bool recursive =
      !!(make_directory_flags & PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          CallCreateDirectory, GetFileSystemContext(), GetFileSystemURL(),
          exclusive, recursive,
          base::BindOnce(&PepperInternalFileRefBackend::DidFinishOnIOThread,
                         weak_factory_.GetWeakPtr(), reply_context,
                         PpapiPluginMsg_FileRef_MakeDirectoryReply())));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Touch(
    ppapi::host::ReplyMessageContext reply_context,
    PP_Time last_access_time_in,
    PP_Time last_modified_time_in) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  base::Time last_access_time = ppapi::PPTimeToTime(last_access_time_in);
  base::Time last_modified_time = ppapi::PPTimeToTime(last_modified_time_in);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          CallTouchFile, GetFileSystemContext(), GetFileSystemURL(),
          last_access_time, last_modified_time,
          base::BindOnce(&PepperInternalFileRefBackend::DidFinishOnIOThread,
                         weak_factory_.GetWeakPtr(), reply_context,
                         PpapiPluginMsg_FileRef_TouchReply())));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Delete(
    ppapi::host::ReplyMessageContext reply_context) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          CallRemove, GetFileSystemContext(), GetFileSystemURL(), false,
          base::BindOnce(&PepperInternalFileRefBackend::DidFinishOnIOThread,
                         weak_factory_.GetWeakPtr(), reply_context,
                         PpapiPluginMsg_FileRef_DeleteReply())));
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

  storage::FileSystemOperationRunner::CopyOrMoveOptionSet options =
      storage::FileSystemOperation::CopyOrMoveOptionSet();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          CallMove, GetFileSystemContext(), GetFileSystemURL(), new_url,
          options,
          base::BindOnce(&PepperInternalFileRefBackend::DidFinishOnIOThread,
                         weak_factory_.GetWeakPtr(), reply_context,
                         PpapiPluginMsg_FileRef_RenameReply())));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperInternalFileRefBackend::Query(
    ppapi::host::ReplyMessageContext reply_context) {
  if (!GetFileSystemURL().is_valid())
    return PP_ERROR_FAILED;

  constexpr storage::FileSystemOperation::GetMetadataFieldSet fields = {
      storage::FileSystemOperation::GetMetadataField::kIsDirectory,
      storage::FileSystemOperation::GetMetadataField::kSize,
      storage::FileSystemOperation::GetMetadataField::kLastModified};
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          CallGetMetadata, GetFileSystemContext(), GetFileSystemURL(), fields,
          base::BindRepeating(
              &PepperInternalFileRefBackend::GetMetadataCompleteOnIOThread,
              weak_factory_.GetWeakPtr(), reply_context)));
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
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          CallReadDirectory, GetFileSystemContext(), GetFileSystemURL(),
          base::BindRepeating(
              &PepperInternalFileRefBackend::ReadDirectoryCompleteOnIOThread,
              weak_factory_.GetWeakPtr(), reply_context,
              base::Owned(accumulated_file_list))));
  return PP_OK_COMPLETIONPENDING;
}

void PepperInternalFileRefBackend::ReadDirectoryComplete(
    ppapi::host::ReplyMessageContext context,
    storage::FileSystemOperation::FileEntryList* accumulated_file_list,
    base::File::Error error,
    storage::FileSystemOperation::FileEntryList file_list,
    bool has_more) {
  accumulated_file_list->insert(accumulated_file_list->end(), file_list.begin(),
                                file_list.end());
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

  host_->SendReply(context, PpapiPluginMsg_FileRef_ReadDirectoryEntriesReply(
                                infos, file_types));
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
