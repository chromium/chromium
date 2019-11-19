// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_flash_file_message_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/pepper/pepper_security_helper.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_path.h"
#include "ppapi/shared_impl/file_type_conversion.h"

namespace content {

namespace {

bool CanRead(int process_id, const base::FilePath& path) {
  return ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(process_id,
                                                                    path);
}

bool CanCreateReadWrite(int process_id, const base::FilePath& path) {
  return ChildProcessSecurityPolicyImpl::GetInstance()->CanCreateReadWriteFile(
      process_id, path);
}

}  // namespace

PepperFlashFileMessageFilter::PepperFlashFileMessageFilter(
    PP_Instance instance,
    BrowserPpapiHost* host)
    : plugin_process_(host->GetPluginProcess().Duplicate()) {
  int unused;
  host->GetRenderFrameIDsForInstance(instance, &render_process_id_, &unused);
  base::FilePath profile_data_directory = host->GetProfileDataDirectory();
  std::string plugin_name = host->GetPluginName();

  if (profile_data_directory.empty() || plugin_name.empty()) {
    // These are used to construct the path. If they are not set it means we
    // will construct a bad path and could provide access to the wrong files.
    // In this case, |plugin_data_directory_| will remain unset and
    // |ValidateAndConvertPepperFilePath| will fail.
    NOTREACHED();
  } else {
    plugin_data_directory_ = GetDataDirName(profile_data_directory).Append(
        base::FilePath::FromUTF8Unsafe(plugin_name));
  }
}

PepperFlashFileMessageFilter::~PepperFlashFileMessageFilter() {}

// static
base::FilePath PepperFlashFileMessageFilter::GetDataDirName(
    const base::FilePath& profile_path) {
  return profile_path.Append(kPepperDataDirname);
}

scoped_refptr<base::TaskRunner>
PepperFlashFileMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  // The blocking pool provides a pool of threads to run file
  // operations, instead of a single thread which might require
  // queuing time.  Since these messages are synchronous as sent from
  // the plugin, the sending thread cannot send a new message until
  // this one returns, so there is no need to sequence tasks here.  If
  // the plugin has multiple threads, it cannot make assumptions about
  // ordering of IPC message sends, so it cannot make assumptions
  // about ordering of operations caused by those IPC messages.
  return scoped_refptr<base::TaskRunner>(base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
}

int32_t PepperFlashFileMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFlashFileMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_OpenFile,
                                      OnOpenFile)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_RenameFile,
                                      OnRenameFile)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_DeleteFileOrDir,
                                      OnDeleteFileOrDir)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_CreateDir,
                                      OnCreateDir)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_QueryFile,
                                      OnQueryFile)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FlashFile_GetDirContents,
                                      OnGetDirContents)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_FlashFile_CreateTemporaryFile, OnCreateTemporaryFile)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashFileMessageFilter::OnOpenFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path,
    int pp_open_flags) {
  base::FilePath full_path = ValidateAndConvertPepperFilePath(
      path, base::Bind(&CanOpenWithPepperFlags, pp_open_flags));
  if (full_path.empty()) {
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_ACCESS_DENIED);
  }

  int platform_file_flags = 0;
  if (!ppapi::PepperFileOpenFlagsToPlatformFileFlags(pp_open_flags,
                                                     &platform_file_flags)) {
    return base::File::FILE_ERROR_FAILED;
  }

  base::File file(full_path, platform_file_flags);
  if (!file.IsValid()) {
    return ppapi::FileErrorToPepperError(file.error_details());
  }

  // Make sure we didn't try to open a directory: directory fd shouldn't be
  // passed to untrusted processes because they open security holes.
  base::File::Info info;
  if (!file.GetInfo(&info) || info.is_directory) {
    // When in doubt, throw it out.
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_ACCESS_DENIED);
  }

  IPC::PlatformFileForTransit transit_file =
      IPC::TakePlatformFileForTransit(std::move(file));
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.AppendHandle(ppapi::proxy::SerializedHandle(
      ppapi::proxy::SerializedHandle::FILE, transit_file));
  SendReply(reply_context, IPC::Message());
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFlashFileMessageFilter::OnRenameFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& from_path,
    const ppapi::PepperFilePath& to_path) {
  base::FilePath from_full_path = ValidateAndConvertPepperFilePath(
      from_path, base::Bind(&CanCreateReadWrite));
  base::FilePath to_full_path = ValidateAndConvertPepperFilePath(
      to_path, base::Bind(&CanCreateReadWrite));
  if (from_full_path.empty() || to_full_path.empty()) {
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_ACCESS_DENIED);
  }

  bool result = base::Move(from_full_path, to_full_path);
  return ppapi::FileErrorToPepperError(
      result ? base::File::FILE_OK : base::File::FILE_ERROR_ACCESS_DENIED);
}

int32_t PepperFlashFileMessageFilter::OnDeleteFileOrDir(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path,
    bool recursive) {
  base::FilePath full_path =
      ValidateAndConvertPepperFilePath(path, base::Bind(&CanCreateReadWrite));
  if (full_path.empty()) {
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_ACCESS_DENIED);
  }

  bool result = base::DeleteFile(full_path, recursive);
  return ppapi::FileErrorToPepperError(
      result ? base::File::FILE_OK : base::File::FILE_ERROR_ACCESS_DENIED);
}
int32_t PepperFlashFileMessageFilter::OnCreateDir(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  base::FilePath full_path =
      ValidateAndConvertPepperFilePath(path, base::Bind(&CanCreateReadWrite));
  if (full_path.empty()) {
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_ACCESS_DENIED);
  }

  bool result = base::CreateDirectory(full_path);
  return ppapi::FileErrorToPepperError(
      result ? base::File::FILE_OK : base::File::FILE_ERROR_ACCESS_DENIED);
}

int32_t PepperFlashFileMessageFilter::OnQueryFile(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  base::FilePath full_path =
      ValidateAndConvertPepperFilePath(path, base::Bind(&CanRead));
  if (full_path.empty()) {
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_ACCESS_DENIED);
  }

  base::File::Info info;
  bool result = base::GetFileInfo(full_path, &info);
  context->reply_msg = PpapiPluginMsg_FlashFile_QueryFileReply(info);
  return ppapi::FileErrorToPepperError(
      result ? base::File::FILE_OK : base::File::FILE_ERROR_ACCESS_DENIED);
}

int32_t PepperFlashFileMessageFilter::OnGetDirContents(
    ppapi::host::HostMessageContext* context,
    const ppapi::PepperFilePath& path) {
  base::FilePath full_path =
      ValidateAndConvertPepperFilePath(path, base::Bind(&CanRead));
  if (full_path.empty()) {
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_ACCESS_DENIED);
  }

  ppapi::DirContents contents;
  base::FileEnumerator enumerator(full_path,
                                  false,
                                  base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES |
                                      base::FileEnumerator::INCLUDE_DOT_DOT);

  while (!enumerator.Next().empty()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    ppapi::DirEntry entry = {info.GetName(), info.IsDirectory()};
    contents.push_back(entry);
  }

  context->reply_msg = PpapiPluginMsg_FlashFile_GetDirContentsReply(contents);
  return PP_OK;
}

int32_t PepperFlashFileMessageFilter::OnCreateTemporaryFile(
    ppapi::host::HostMessageContext* context) {
  ppapi::PepperFilePath dir_path(ppapi::PepperFilePath::DOMAIN_MODULE_LOCAL,
                                 base::FilePath());
  base::FilePath validated_dir_path = ValidateAndConvertPepperFilePath(
      dir_path, base::Bind(&CanCreateReadWrite));
  if (validated_dir_path.empty() ||
      (!base::DirectoryExists(validated_dir_path) &&
       !base::CreateDirectory(validated_dir_path))) {
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_ACCESS_DENIED);
  }

  base::FilePath file_path;
  if (!base::CreateTemporaryFileInDir(validated_dir_path, &file_path)) {
    return ppapi::FileErrorToPepperError(base::File::FILE_ERROR_FAILED);
  }

  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                      base::File::FLAG_WRITE | base::File::FLAG_TEMPORARY |
                      base::File::FLAG_DELETE_ON_CLOSE);

  if (!file.IsValid())
    return ppapi::FileErrorToPepperError(file.error_details());

  IPC::PlatformFileForTransit transit_file =
      IPC::TakePlatformFileForTransit(std::move(file));
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.AppendHandle(ppapi::proxy::SerializedHandle(
      ppapi::proxy::SerializedHandle::FILE, transit_file));
  SendReply(reply_context, IPC::Message());
  return PP_OK_COMPLETIONPENDING;
}

base::FilePath PepperFlashFileMessageFilter::ValidateAndConvertPepperFilePath(
    const ppapi::PepperFilePath& pepper_path,
    const CheckPermissionsCallback& check_permissions_callback) const {
  base::FilePath file_path;  // Empty path returned on error.
  switch (pepper_path.domain()) {
    case ppapi::PepperFilePath::DOMAIN_ABSOLUTE:
      if (pepper_path.path().IsAbsolute() &&
          check_permissions_callback.Run(render_process_id_,
                                         pepper_path.path()))
        file_path = pepper_path.path();
      break;
    case ppapi::PepperFilePath::DOMAIN_MODULE_LOCAL:
      // This filter provides the module name portion of the path to prevent
      // plugins from accessing each other's data.
      if (!plugin_data_directory_.empty() && !pepper_path.path().IsAbsolute() &&
          !pepper_path.path().ReferencesParent())
        file_path = plugin_data_directory_.Append(pepper_path.path());
      break;
    default:
      NOTREACHED();
      break;
  }
  return file_path;
}

}  // namespace content
