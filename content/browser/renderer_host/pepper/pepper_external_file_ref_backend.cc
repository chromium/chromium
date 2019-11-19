// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_external_file_ref_backend.h"

#include <string.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_thread.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"

namespace content {

namespace {

struct GetFileInfoResults {
  base::File::Error error;
  base::File::Info info;
};

using GetFileInfoCallback =
    base::OnceCallback<void(base::File::Error, const base::File::Info&)>;

GetFileInfoResults DoGetFileInfo(const base::FilePath& path) {
  GetFileInfoResults results;
  if (!base::PathExists(path)) {
    results.error = base::File::FILE_ERROR_NOT_FOUND;
    return results;
  }
  results.error = base::GetFileInfo(path, &results.info)
                      ? base::File::FILE_OK
                      : base::File::FILE_ERROR_FAILED;
  return results;
}

void SendGetFileInfoResults(GetFileInfoCallback callback,
                            const GetFileInfoResults& results) {
  std::move(callback).Run(results.error, results.info);
}

base::File::Error CallTouchFile(const base::FilePath& path,
                                PP_Time last_access_time,
                                PP_Time last_modified_time) {
  bool result = base::TouchFile(path, ppapi::PPTimeToTime(last_access_time),
                                ppapi::PPTimeToTime(last_modified_time));
  return result ? base::File::FILE_OK : base::File::FILE_ERROR_FAILED;
}

}  // namespace

PepperExternalFileRefBackend::PepperExternalFileRefBackend(
    ppapi::host::PpapiHost* host,
    int render_process_id,
    const base::FilePath& path)
    : host_(host),
      path_(path),
      render_process_id_(render_process_id),
      task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

PepperExternalFileRefBackend::~PepperExternalFileRefBackend() {}

int32_t PepperExternalFileRefBackend::MakeDirectory(
    ppapi::host::ReplyMessageContext reply_context,
    int32_t make_directory_flags) {
  // This operation isn't supported for external filesystems.
  return PP_ERROR_NOACCESS;
}

int32_t PepperExternalFileRefBackend::Touch(
    ppapi::host::ReplyMessageContext reply_context,
    PP_Time last_access_time,
    PP_Time last_modified_time) {
  IPC::Message reply_msg = PpapiPluginMsg_FileRef_TouchReply();
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      BindOnce(&CallTouchFile, path_, last_access_time, last_modified_time),
      base::BindOnce(&PepperExternalFileRefBackend::DidFinish,
                     weak_factory_.GetWeakPtr(), reply_context, reply_msg));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperExternalFileRefBackend::Delete(
    ppapi::host::ReplyMessageContext reply_context) {
  // This operation isn't supported for external filesystems.
  return PP_ERROR_NOACCESS;
}

int32_t PepperExternalFileRefBackend::Rename(
    ppapi::host::ReplyMessageContext reply_context,
    PepperFileRefHost* new_file_ref) {
  // This operation isn't supported for external filesystems.
  return PP_ERROR_NOACCESS;
}

int32_t PepperExternalFileRefBackend::Query(
    ppapi::host::ReplyMessageContext reply_context) {
  bool ok = base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE, base::BindOnce(&DoGetFileInfo, path_),
      base::BindOnce(
          &SendGetFileInfoResults,
          base::BindOnce(&PepperExternalFileRefBackend::GetMetadataComplete,
                         weak_factory_.GetWeakPtr(), reply_context)));
  DCHECK(ok);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperExternalFileRefBackend::ReadDirectoryEntries(
    ppapi::host::ReplyMessageContext context) {
  // This operation isn't supported for external filesystems.
  return PP_ERROR_NOACCESS;
}

int32_t PepperExternalFileRefBackend::GetAbsolutePath(
    ppapi::host::ReplyMessageContext reply_context) {
  host_->SendReply(
      reply_context,
      PpapiPluginMsg_FileRef_GetAbsolutePathReply(path_.AsUTF8Unsafe()));

  // Use PP_OK_COMPLETIONPENDING instead of PP_OK since we've already sent our
  // reply above.
  return PP_OK_COMPLETIONPENDING;
}

storage::FileSystemURL PepperExternalFileRefBackend::GetFileSystemURL() const {
  return storage::FileSystemURL();
}

base::FilePath PepperExternalFileRefBackend::GetExternalFilePath() const {
  return path_;
}

int32_t PepperExternalFileRefBackend::CanRead() const {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
          render_process_id_, path_)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

int32_t PepperExternalFileRefBackend::CanWrite() const {
  // Platform files have coarse-grained grants in ChildProcessSecurityPolicy.
  return CanReadWrite();
}

int32_t PepperExternalFileRefBackend::CanCreate() const {
  // Platform files have coarse-grained grants in ChildProcessSecurityPolicy.
  return CanReadWrite();
}

int32_t PepperExternalFileRefBackend::CanReadWrite() const {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanCreateReadWriteFile(
          render_process_id_, path_)) {
    return PP_ERROR_NOACCESS;
  }
  return PP_OK;
}

void PepperExternalFileRefBackend::DidFinish(
    ppapi::host::ReplyMessageContext reply_context,
    const IPC::Message& msg,
    base::File::Error error) {
  reply_context.params.set_result(ppapi::FileErrorToPepperError(error));
  host_->SendReply(reply_context, msg);
}

void PepperExternalFileRefBackend::GetMetadataComplete(
    ppapi::host::ReplyMessageContext reply_context,
    const base::File::Error error,
    const base::File::Info& file_info) {
  reply_context.params.set_result(ppapi::FileErrorToPepperError(error));

  PP_FileInfo pp_file_info;
  if (error == base::File::FILE_OK) {
    ppapi::FileInfoToPepperFileInfo(
        file_info, PP_FILESYSTEMTYPE_EXTERNAL, &pp_file_info);
  } else {
    memset(&pp_file_info, 0, sizeof(pp_file_info));
  }

  host_->SendReply(reply_context,
                   PpapiPluginMsg_FileRef_QueryReply(pp_file_info));
}

}  // namespace content
