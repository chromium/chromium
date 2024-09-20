// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_file_io_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/browser/renderer_host/pepper/pepper_security_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_system_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/common/file_system/file_system_util.h"

namespace content {

using ppapi::FileIOStateManager;
using ppapi::PPTimeToTime;

namespace {

PepperFileIOHost::UIThreadStuff GetUIThreadStuffForInternalFileSystems(
    int render_process_id) {
  PepperFileIOHost::UIThreadStuff stuff;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (host) {
    stuff.resolved_render_process_id = host->GetProcess().Pid();
    StoragePartition* storage_partition = host->GetStoragePartition();
    if (storage_partition)
      stuff.file_system_context = storage_partition->GetFileSystemContext();
  }
  return stuff;
}

base::ProcessId GetResolvedRenderProcessId(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (!host)
    return base::kNullProcessId;
  return host->GetProcess().Pid();
}

bool GetPluginAllowedToCallRequestOSFileHandle(int render_process_id,
                                               const GURL& document_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ContentBrowserClient* client = GetContentClient()->browser();
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (!host)
    return false;
  return client->IsPluginAllowedToCallRequestOSFileHandle(
      host->GetBrowserContext(), document_url);
}

bool FileOpenForWrite(int32_t open_flags) {
  return (open_flags & (PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_APPEND)) != 0;
}

void FileCloser(base::File auto_close) {
}

void DidCloseFile(base::ScopedClosureRunner on_close_callback) {
  on_close_callback.RunAndReset();
}

void DidOpenFile(base::WeakPtr<PepperFileIOHost> file_host,
                 scoped_refptr<base::SequencedTaskRunner> task_runner,
                 storage::FileSystemOperation::OpenFileCallback callback,
                 base::File file,
                 base::ScopedClosureRunner on_close_callback) {
  if (file_host) {
    std::move(callback).Run(std::move(file), on_close_callback.Release());
  } else {
    task_runner->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&FileCloser, std::move(file)),
        base::BindOnce(&DidCloseFile, std::move(on_close_callback)));
  }
}

void OpenFileCallbackWrapperIO(
    storage::FileSystemOperationRunner::OpenFileCallback callback,
    base::File file,
    base::ScopedClosureRunner on_close_callback) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(file),
                                std::move(on_close_callback)));
}

void CallOpenFile(
    PepperFileSystemBrowserHost::GetOperationRunnerCallback get_runner,
    const storage::FileSystemURL& url,
    uint32_t file_flags,
    storage::FileSystemOperationRunner::OpenFileCallback callback) {
  get_runner.Run()->OpenFile(
      url, file_flags,
      base::BindOnce(&OpenFileCallbackWrapperIO, std::move(callback)));
}

}  // namespace

PepperFileIOHost::PepperFileIOHost(BrowserPpapiHostImpl* host,
                                   PP_Instance instance,
                                   PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      browser_ppapi_host_(host),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      file_(task_runner_.get()),
      open_flags_(0),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      max_written_offset_(0),
      check_quota_(false) {
  int unused;
  if (!host->GetRenderFrameIDsForInstance(
          instance, &render_process_id_, &unused)) {
    render_process_id_ = -1;
  }
}

PepperFileIOHost::~PepperFileIOHost() {}

int32_t PepperFileIOHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFileIOHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Open, OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Touch, OnHostMsgTouch)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_SetLength,
                                      OnHostMsgSetLength)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_Flush,
                                        OnHostMsgFlush)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Close, OnHostMsgClose)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_RequestOSFileHandle,
                                        OnHostMsgRequestOSFileHandle)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

PepperFileIOHost::UIThreadStuff::UIThreadStuff() {
  resolved_render_process_id = base::kNullProcessId;
}

PepperFileIOHost::UIThreadStuff::UIThreadStuff(const UIThreadStuff& other) =
    default;

PepperFileIOHost::UIThreadStuff::~UIThreadStuff() {}

int32_t PepperFileIOHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    PP_Resource file_ref_resource,
    int32_t open_flags) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, false);
  if (rv != PP_OK)
    return rv;

  uint32_t platform_file_flags = 0;
  if (!ppapi::PepperFileOpenFlagsToPlatformFileFlags(open_flags,
                                                     &platform_file_flags))
    return PP_ERROR_BADARGUMENT;

  ppapi::host::ResourceHost* resource_host =
      host()->GetResourceHost(file_ref_resource);
  if (!resource_host || !resource_host->IsFileRefHost())
    return PP_ERROR_BADRESOURCE;
  PepperFileRefHost* file_ref_host =
      static_cast<PepperFileRefHost*>(resource_host);
  if (file_ref_host->GetFileSystemType() == PP_FILESYSTEMTYPE_INVALID)
    return PP_ERROR_FAILED;

  file_system_host_ = file_ref_host->GetFileSystemHost();

  open_flags_ = open_flags;
  file_system_type_ = file_ref_host->GetFileSystemType();
  file_system_url_ = file_ref_host->GetFileSystemURL();

  // For external file systems, if there is a valid FileSystemURL, then treat
  // it like internal file systems and access it via the FileSystemURL.
  bool is_internal_type = (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) ||
                          file_system_url_.is_valid();

  if (is_internal_type) {
    if (!file_system_url_.is_valid())
      return PP_ERROR_BADARGUMENT;

    // Not all external file systems are fully supported yet.
    // Whitelist the supported ones.
    if (file_system_url_.mount_type() == storage::kFileSystemTypeExternal) {
      switch (file_system_url_.type()) {
        case storage::kFileSystemTypeLocalMedia:
        case storage::kFileSystemTypeDeviceMedia:
          break;
        default:
          return PP_ERROR_NOACCESS;
      }
    }
    if (!CanOpenFileSystemURLWithPepperFlags(
            open_flags, render_process_id_, file_system_url_))
      return PP_ERROR_NOACCESS;

    GotUIThreadStuffForInternalFileSystems(
        context->MakeReplyMessageContext(), platform_file_flags,
        GetUIThreadStuffForInternalFileSystems(render_process_id_));
  } else {
    base::FilePath path = file_ref_host->GetExternalFilePath();
    if (!CanOpenWithPepperFlags(open_flags, render_process_id_, path))
      return PP_ERROR_NOACCESS;
    GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GetResolvedRenderProcessId, render_process_id_),
        base::BindOnce(&PepperFileIOHost::GotResolvedRenderProcessId,
                       weak_ptr_factory_.GetWeakPtr(),
                       context->MakeReplyMessageContext(), path,
                       platform_file_flags));
  }
  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileIOHost::GotUIThreadStuffForInternalFileSystems(
    ppapi::host::ReplyMessageContext reply_context,
    uint32_t platform_file_flags,
    UIThreadStuff ui_thread_stuff) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  resolved_render_process_id_ = ui_thread_stuff.resolved_render_process_id;
  if (resolved_render_process_id_ == base::kNullProcessId ||
      !ui_thread_stuff.file_system_context.get()) {
    reply_context.params.set_result(PP_ERROR_FAILED);
    SendOpenErrorReply(reply_context);
    return;
  }

  if (!ui_thread_stuff.file_system_context->GetFileSystemBackend(
          file_system_url_.type())) {
    reply_context.params.set_result(PP_ERROR_FAILED);
    SendOpenErrorReply(reply_context);
    return;
  }

  if (!file_system_host_.get()) {
    reply_context.params.set_result(PP_ERROR_FAILED);
    SendOpenErrorReply(reply_context);
    return;
  }

  auto open_callback = base::BindOnce(
      &DidOpenFile, weak_ptr_factory_.GetWeakPtr(), task_runner_,
      base::BindOnce(&PepperFileIOHost::DidOpenInternalFile,
                     weak_ptr_factory_.GetWeakPtr(), reply_context));
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          CallOpenFile, file_system_host_->GetFileSystemOperationRunner(),
          file_system_url_, platform_file_flags, std::move(open_callback)));
}

void PepperFileIOHost::DidOpenInternalFile(
    ppapi::host::ReplyMessageContext reply_context,
    base::File file,
    base::OnceClosure on_close_callback) {
  if (file.IsValid()) {
    base::ScopedClosureRunner scoped_runner(std::move(on_close_callback));
    on_close_callback_ = std::move(scoped_runner);

    if (FileOpenForWrite(open_flags_) && file_system_host_->ChecksQuota()) {
      check_quota_ = true;
      file_system_host_->OpenQuotaFile(
          this, file_system_url_,
          base::BindOnce(&PepperFileIOHost::DidOpenQuotaFile,
                         weak_ptr_factory_.GetWeakPtr(), reply_context,
                         std::move(file)));
      return;
    }
  }

  DCHECK(!file_.IsValid());
  base::File::Error error =
      file.IsValid() ? base::File::FILE_OK : file.error_details();
  file_.SetFile(std::move(file));
  SendFileOpenReply(reply_context, error);
}

void PepperFileIOHost::GotResolvedRenderProcessId(
    ppapi::host::ReplyMessageContext reply_context,
    base::FilePath path,
    uint32_t file_flags,
    base::ProcessId resolved_render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  resolved_render_process_id_ = resolved_render_process_id;
  file_.CreateOrOpen(
      path, file_flags,
      base::BindOnce(&PepperFileIOHost::OnLocalFileOpened,
                     weak_ptr_factory_.GetWeakPtr(), reply_context, path));
}

int32_t PepperFileIOHost::OnHostMsgTouch(
    ppapi::host::HostMessageContext* context,
    PP_Time last_access_time,
    PP_Time last_modified_time) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  if (!file_.SetTimes(
          PPTimeToTime(last_access_time), PPTimeToTime(last_modified_time),
          base::BindOnce(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                         weak_ptr_factory_.GetWeakPtr(),
                         context->MakeReplyMessageContext()))) {
    return PP_ERROR_FAILED;
  }

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgSetLength(
    ppapi::host::HostMessageContext* context,
    int64_t length) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;
  if (length < 0)
    return PP_ERROR_BADARGUMENT;

  // Quota checks are performed on the plugin side, in order to use the same
  // quota reservation and request system as Write.

  if (!file_.SetLength(
          length,
          base::BindOnce(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                         weak_ptr_factory_.GetWeakPtr(),
                         context->MakeReplyMessageContext()))) {
    return PP_ERROR_FAILED;
  }

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgFlush(
    ppapi::host::HostMessageContext* context) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  if (!file_.Flush(
          base::BindOnce(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                         weak_ptr_factory_.GetWeakPtr(),
                         context->MakeReplyMessageContext()))) {
    return PP_ERROR_FAILED;
  }

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context,
    const ppapi::FileGrowth& file_growth) {
  if (check_quota_) {
    file_system_host_->CloseQuotaFile(this, file_growth);
    check_quota_ = false;
  }

  if (file_.IsValid()) {
    file_.Close(base::BindOnce(&PepperFileIOHost::DidCloseFile,
                               weak_ptr_factory_.GetWeakPtr()));
  }
  return PP_OK;
}

void PepperFileIOHost::DidOpenQuotaFile(
    ppapi::host::ReplyMessageContext reply_context,
    base::File file,
    int64_t max_written_offset) {
  DCHECK(!file_.IsValid());
  DCHECK(file.IsValid());
  max_written_offset_ = max_written_offset;
  file_.SetFile(std::move(file));

  SendFileOpenReply(reply_context, base::File::FILE_OK);
}

void PepperFileIOHost::DidCloseFile(base::File::Error /*error*/) {
  // Silently ignore if we fail to close the file.
  on_close_callback_.RunAndReset();
}

int32_t PepperFileIOHost::OnHostMsgRequestOSFileHandle(
    ppapi::host::HostMessageContext* context) {
  if (open_flags_ != PP_FILEOPENFLAG_READ && file_system_host_->ChecksQuota())
    return PP_ERROR_FAILED;

  GURL document_url =
      browser_ppapi_host_->GetDocumentURLForInstance(pp_instance());
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetPluginAllowedToCallRequestOSFileHandle,
                     render_process_id_, document_url),
      base::BindOnce(
          &PepperFileIOHost::GotPluginAllowedToCallRequestOSFileHandle,
          weak_ptr_factory_.GetWeakPtr(), context->MakeReplyMessageContext()));
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileIOHost::GotPluginAllowedToCallRequestOSFileHandle(
    ppapi::host::ReplyMessageContext reply_context,
    bool plugin_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_ppapi_host_->external_plugin() ||
      host()->permissions().HasPermission(ppapi::PERMISSION_PRIVATE) ||
      plugin_allowed) {
    if (!AddFileToReplyContext(open_flags_, &reply_context))
      reply_context.params.set_result(PP_ERROR_FAILED);
  } else {
    reply_context.params.set_result(PP_ERROR_NOACCESS);
  }
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FileIO_RequestOSFileHandleReply());
}

void PepperFileIOHost::ExecutePlatformGeneralCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::File::Error error_code) {
  reply_context.params.set_result(ppapi::FileErrorToPepperError(error_code));
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_GeneralReply());
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::OnLocalFileOpened(
    ppapi::host::ReplyMessageContext reply_context,
    const base::FilePath& path,
    base::File::Error error_code) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Quarantining a file before its contents are available is only supported on
  // Windows and Linux.
  if (!FileOpenForWrite(open_flags_) || error_code != base::File::FILE_OK) {
    SendFileOpenReply(reply_context, error_code);
    return;
  }

  mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote;
  download::QuarantineConnectionCallback quarantine_connection_callback =
      GetContentClient()->browser()->GetQuarantineConnectionCallback();
  if (quarantine_connection_callback) {
    quarantine_connection_callback.Run(
        quarantine_remote.BindNewPipeAndPassReceiver());
  }

  if (quarantine_remote) {
    quarantine::mojom::Quarantine* raw_quarantine = quarantine_remote.get();
    raw_quarantine->QuarantineFile(
        path, browser_ppapi_host_->GetDocumentURLForInstance(pp_instance()),
        GURL(), /*request_initiator=*/std::nullopt, std::string(),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&PepperFileIOHost::OnLocalFileQuarantined,
                           weak_ptr_factory_.GetWeakPtr(), reply_context, path,
                           std::move(quarantine_remote)),
            quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED));
  } else {
    SendFileOpenReply(reply_context, error_code);
  }
#else
  SendFileOpenReply(reply_context, error_code);
#endif
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void PepperFileIOHost::OnLocalFileQuarantined(
    ppapi::host::ReplyMessageContext reply_context,
    const base::FilePath& path,
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
    quarantine::mojom::QuarantineFileResult quarantine_result) {
  base::File::Error file_error =
      (quarantine_result == quarantine::mojom::QuarantineFileResult::OK
           ? base::File::FILE_OK
           : base::File::FILE_ERROR_SECURITY);
  if (file_error != base::File::FILE_OK && file_.IsValid())
    file_.Close(base::FileProxy::StatusCallback());
  SendFileOpenReply(reply_context, file_error);
}
#endif

void PepperFileIOHost::SendFileOpenReply(
    ppapi::host::ReplyMessageContext reply_context,
    base::File::Error error_code) {
  int32_t pp_error = ppapi::FileErrorToPepperError(error_code);
  if (file_.IsValid() && !AddFileToReplyContext(open_flags_, &reply_context))
    pp_error = PP_ERROR_FAILED;

  PP_Resource quota_file_system = 0;
  if (pp_error == PP_OK) {
    state_manager_.SetOpenSucceed();
    // A non-zero resource id signals the plugin side to check quota.
    if (check_quota_)
      quota_file_system = file_system_host_->pp_resource();
  }

  reply_context.params.set_result(pp_error);
  host()->SendReply(
      reply_context,
      PpapiPluginMsg_FileIO_OpenReply(quota_file_system, max_written_offset_));
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::SendOpenErrorReply(
    ppapi::host::ReplyMessageContext reply_context) {
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_OpenReply(0, 0));
}

bool PepperFileIOHost::AddFileToReplyContext(
    int32_t open_flags,
    ppapi::host::ReplyMessageContext* reply_context) const {
  IPC::PlatformFileForTransit transit_file =
      IPC::GetPlatformFileForTransit(file_.GetPlatformFile(), false);
  if (transit_file == IPC::InvalidPlatformFileForTransit())
    return false;

  ppapi::proxy::SerializedHandle file_handle;
  // A non-zero resource id signals NaClIPCAdapter to create a NaClQuotaDesc.
  PP_Resource quota_file_io = check_quota_ ? pp_resource() : 0;
  file_handle.set_file_handle(transit_file, open_flags, quota_file_io);
  reply_context->params.AppendHandle(std::move(file_handle));
  return true;
}

}  // namespace content
