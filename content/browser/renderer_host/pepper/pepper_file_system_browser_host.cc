// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/pepper/pepper_file_io_host.h"
#include "content/browser/renderer_host/pepper/quota_reservation.h"
#include "content/common/pepper_file_util.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_plugin_info.h"
#include "net/base/mime_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_system_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// This is the minimum amount of quota we reserve per file system.
const int64_t kMinimumQuotaReservationSize = 1024 * 1024;  // 1 MB

storage::FileSystemType PepperFileSystemTypeToFileSystemType(
    PP_FileSystemType type) {
  switch (type) {
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
      return storage::kFileSystemTypeTemporary;
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      return storage::kFileSystemTypePersistent;
    case PP_FILESYSTEMTYPE_EXTERNAL:
      return storage::kFileSystemTypeExternal;
    default:
      return storage::kFileSystemTypeUnknown;
  }
}

void RunOpenQuotaCallbackOnUI(
    PepperFileSystemBrowserHost::OpenQuotaFileCallback callback,
    int64_t max_written_offset) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), max_written_offset));
}

void RunReserveQuotaCallbackOnUI(
    QuotaReservation::ReserveQuotaCallback callback,
    int64_t amount,
    const ppapi::FileSizeMap& file_sizes) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), amount, file_sizes));
}

}  // namespace

PepperFileSystemBrowserHost::IOThreadState::IOThreadState(
    PP_FileSystemType type,
    base::WeakPtr<PepperFileSystemBrowserHost> host)
    : type_(type),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      host_(host) {}

PepperFileSystemBrowserHost::IOThreadState::~IOThreadState() {
  // All FileRefs and FileIOs that reference us must have been destroyed. Cancel
  // all pending file system operations.
  if (file_system_operation_runner_)
    file_system_operation_runner_->Shutdown();
}

void PepperFileSystemBrowserHost::IOThreadState::OpenExistingFileSystem(
    const GURL& root_url,
    base::OnceClosure callback,
    scoped_refptr<storage::FileSystemContext> file_system_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  root_url_ = root_url;
  if (file_system_context.get()) {
    opened_ = true;
  } else {
    // If there is no file system context, we log a warning and continue with an
    // invalid resource (which will produce errors when used), since we have no
    // way to communicate the error to the caller.
    LOG(WARNING) << "Could not retrieve file system context.";
  }
  SetFileSystemContext(file_system_context);

  ShouldCreateQuotaReservation(base::BindOnce(
      [](scoped_refptr<IOThreadState> io_thread_state,
         base::OnceClosure callback, bool should_create_quota_reservation) {
        if (should_create_quota_reservation) {
          io_thread_state->CreateQuotaReservation(std::move(callback));
        } else {
          io_thread_state->RunCallbackIfHostAlive(std::move(callback));
        }
      },
      base::WrapRefCounted(this), std::move(callback)));
}

void PepperFileSystemBrowserHost::IOThreadState::OpenFileSystem(
    const GURL& origin,
    ppapi::host::ReplyMessageContext reply_context,
    storage::FileSystemType file_system_type,
    scoped_refptr<storage::FileSystemContext> file_system_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!file_system_context.get()) {
    OpenFileSystemComplete(reply_context, storage::FileSystemURL(),
                           std::string(), base::File::FILE_ERROR_FAILED);
    return;
  }

  SetFileSystemContext(file_system_context);

  // TODO(crbug.com/40782681): figure out if StorageKey conversion
  // should replaced with a third-party value: is ppapi only limited to
  // first-party contexts? If so, the implementation below is correct.
  file_system_context_->OpenFileSystem(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin)),
      /*bucket=*/std::nullopt, file_system_type,
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindOnce(&IOThreadState::OpenFileSystemComplete, this,
                     reply_context));
}

void PepperFileSystemBrowserHost::IOThreadState::OpenIsolatedFileSystem(
    const GURL& origin,
    const GURL& root_url,
    const std::string& plugin_id,
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& fsid,
    PP_IsolatedFileSystemType_Private type,
    scoped_refptr<storage::FileSystemContext> file_system_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!file_system_context.get()) {
    SendReplyForIsolatedFileSystem(reply_context, fsid, PP_ERROR_FAILED);
    return;
  }
  SetFileSystemContext(file_system_context);

  if (!root_url_.is_valid()) {
    SendReplyForIsolatedFileSystem(reply_context, fsid, PP_ERROR_FAILED);
    return;
  }

  switch (type) {
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX:
      opened_ = true;
      SendReplyForIsolatedFileSystem(reply_context, fsid, PP_OK);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      SendReplyForIsolatedFileSystem(reply_context, fsid, PP_ERROR_BADARGUMENT);
      return;
  }
}

void PepperFileSystemBrowserHost::IOThreadState::OpenFileSystemComplete(
    ppapi::host::ReplyMessageContext reply_context,
    const storage::FileSystemURL& root,
    const std::string& name,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int32_t pp_error = ppapi::FileErrorToPepperError(error);
  if (pp_error == PP_OK) {
    opened_ = true;
    // TODO(crbug.com/40838958): Store and use FileSystemURL instead.
    root_url_ = root.ToGURL();

    ShouldCreateQuotaReservation(base::BindOnce(
        [](scoped_refptr<IOThreadState> io_thread_state,
           ppapi::host::ReplyMessageContext reply_context,
           bool should_create_quota_reservation) {
          if (should_create_quota_reservation) {
            io_thread_state->CreateQuotaReservation(base::BindOnce(
                &IOThreadState::SendReplyForFileSystemIfHostAlive,
                io_thread_state, reply_context, static_cast<int32_t>(PP_OK)));
          } else {
            io_thread_state->SendReplyForFileSystemIfHostAlive(reply_context,
                                                               PP_OK);
          }
        },
        base::WrapRefCounted(this), reply_context));
    return;
  }
  SendReplyForFileSystemIfHostAlive(reply_context, pp_error);
}

void PepperFileSystemBrowserHost::IOThreadState::RunCallbackIfHostAlive(
    base::OnceClosure callback) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadState::RunCallbackIfHostAlive, this,
                                  std::move(callback)));
    return;
  }

  if (!host_)
    return;

  std::move(callback).Run();
}

void PepperFileSystemBrowserHost::IOThreadState::
    SendReplyForFileSystemIfHostAlive(
        ppapi::host::ReplyMessageContext reply_context,
        int32_t pp_error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&IOThreadState::SendReplyForFileSystemIfHostAlive, this,
                       reply_context, pp_error));
    return;
  }

  if (!host_)
    return;

  reply_context.params.set_result(pp_error);
  host_->host()->SendReply(reply_context,
                           PpapiPluginMsg_FileSystem_OpenReply());
}

void PepperFileSystemBrowserHost::IOThreadState::SendReplyForIsolatedFileSystem(
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& fsid,
    int32_t error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&IOThreadState::SendReplyForIsolatedFileSystem, this,
                       reply_context, fsid, error));
    return;
  }

  if (!host_)
    return;

  if (error != PP_OK)
    storage::IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
  reply_context.params.set_result(error);
  host_->SendReply(reply_context,
                   PpapiPluginMsg_FileSystem_InitIsolatedFileSystemReply());
}

void PepperFileSystemBrowserHost::IOThreadState::SetFileSystemContext(
    scoped_refptr<storage::FileSystemContext> file_system_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context_ = file_system_context;
  if (type_ != PP_FILESYSTEMTYPE_EXTERNAL || root_url_.is_valid()) {
    file_system_operation_runner_ =
        file_system_context_->CreateFileSystemOperationRunner();
  }
}

void PepperFileSystemBrowserHost::IOThreadState::ShouldCreateQuotaReservation(
    base::OnceCallback<void(bool)> callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Some file system types don't have quota.
  if (!ppapi::FileSystemTypeHasQuota(type_)) {
    std::move(callback).Run(false);
    return;
  }

  // For file system types with quota, some origins have unlimited storage.
  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy =
      file_system_context_->quota_manager_proxy();
  CHECK(quota_manager_proxy);
  storage::FileSystemType file_system_type =
      PepperFileSystemTypeToFileSystemType(type_);
  quota_manager_proxy->IsStorageUnlimited(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(root_url_)),
      storage::FileSystemTypeToQuotaStorageType(file_system_type),
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             bool is_storage_unlimited) {
            std::move(callback).Run(!is_storage_unlimited);
          },
          std::move(callback)));
}

void PepperFileSystemBrowserHost::IOThreadState::CreateQuotaReservation(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(root_url_.is_valid());
  file_system_context_->default_file_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&QuotaReservation::Create, file_system_context_,
                     root_url_.DeprecatedGetOriginAsURL(),
                     PepperFileSystemTypeToFileSystemType(type_)),
      base::BindOnce(&IOThreadState::GotQuotaReservation, this,
                     std::move(callback)));
}

void PepperFileSystemBrowserHost::IOThreadState::GotQuotaReservation(
    base::OnceClosure callback,
    scoped_refptr<QuotaReservation> quota_reservation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperFileSystemBrowserHost::GotQuotaReservation, host_,
                     std::move(callback), quota_reservation));
}

PepperFileSystemBrowserHost::PepperFileSystemBrowserHost(BrowserPpapiHost* host,
                                                         PP_Instance instance,
                                                         PP_Resource resource,
                                                         PP_FileSystemType type)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      browser_ppapi_host_(host),
      type_(type),
      called_open_(false),
      reserved_quota_(0),
      reserving_quota_(false) {
  io_thread_state_ =
      base::MakeRefCounted<IOThreadState>(type, weak_factory_.GetWeakPtr());
}

PepperFileSystemBrowserHost::~PepperFileSystemBrowserHost() {
  // If |files_| is not empty, the plugin failed to close some files. It must
  // have crashed.
  if (!files_.empty()) {
    io_thread_state_->file_system_context()
        ->default_file_task_runner()
        ->PostTask(FROM_HERE, base::BindOnce(&QuotaReservation::OnClientCrash,
                                             quota_reservation_));
  }
}

void PepperFileSystemBrowserHost::OpenExisting(const GURL& root_url,
                                               base::OnceClosure callback) {
  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderFrameIDsForInstance(
          pp_instance(), &render_process_id, &unused)) {
    NOTREACHED_IN_MIGRATION();
  }
  called_open_ = true;
  // Get the file system context asynchronously, and then complete the Open
  // operation by calling |callback|.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IOThreadState::OpenExistingFileSystem, io_thread_state_,
                     root_url, std::move(callback),
                     GetFileSystemContextFromRenderId(render_process_id)));
}

int32_t PepperFileSystemBrowserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFileSystemBrowserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileSystem_Open,
                                      OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FileSystem_InitIsolatedFileSystem,
        OnHostMsgInitIsolatedFileSystem)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileSystem_ReserveQuota,
                                      OnHostMsgReserveQuota)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperFileSystemBrowserHost::IsFileSystemHost() {
  return true;
}

bool PepperFileSystemBrowserHost::IsOpened() const {
  DCHECK(called_open_);
  return io_thread_state_->opened();
}

GURL PepperFileSystemBrowserHost::GetRootUrl() const {
  DCHECK(called_open_);
  return io_thread_state_->root_url();
}

PepperFileSystemBrowserHost::GetOperationRunnerCallback
PepperFileSystemBrowserHost::GetFileSystemOperationRunner() const {
  return base::BindRepeating(
      &PepperFileSystemBrowserHost::GetFileSystemOperationRunnerInternal,
      io_thread_state_);
}

void PepperFileSystemBrowserHost::OpenQuotaFile(
    PepperFileIOHost* file_io_host,
    const storage::FileSystemURL& url,
    OpenQuotaFileCallback callback) {
  int32_t id = file_io_host->pp_resource();
  std::pair<FileMap::iterator, bool> insert_result =
      files_.insert(std::make_pair(id, file_io_host));
  if (!insert_result.second) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  io_thread_state_->file_system_context()
      ->default_file_task_runner()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&QuotaReservation::OpenFile, quota_reservation_, id,
                         url),
          base::BindOnce(RunOpenQuotaCallbackOnUI, std::move(callback)));
}

void PepperFileSystemBrowserHost::CloseQuotaFile(
    PepperFileIOHost* file_io_host,
    const ppapi::FileGrowth& file_growth) {
  int32_t id = file_io_host->pp_resource();
  auto it = files_.find(id);
  if (it != files_.end()) {
    files_.erase(it);
  } else {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  io_thread_state_->file_system_context()->default_file_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&QuotaReservation::CloseFile,
                                quota_reservation_, id, file_growth));
}

scoped_refptr<storage::FileSystemContext>
PepperFileSystemBrowserHost::GetFileSystemContextFromRenderId(
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (!host)
    return nullptr;
  StoragePartition* storage_partition = host->GetStoragePartition();
  if (!storage_partition)
    return nullptr;
  return storage_partition->GetFileSystemContext();
}

int32_t PepperFileSystemBrowserHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    int64_t /* unused */) {
  // TODO(raymes): The file system size is now unused by FileSystemDispatcher.
  // Figure out why. Why is the file system size signed?

  // Not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  storage::FileSystemType file_system_type =
      PepperFileSystemTypeToFileSystemType(type_);
  if (file_system_type == storage::kFileSystemTypeUnknown)
    return PP_ERROR_FAILED;

  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderFrameIDsForInstance(
          pp_instance(), &render_process_id, &unused)) {
    return PP_ERROR_FAILED;
  }

  GURL origin = browser_ppapi_host_->GetDocumentURLForInstance(pp_instance())
                    .DeprecatedGetOriginAsURL();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IOThreadState::OpenFileSystem, io_thread_state_, origin,
                     context->MakeReplyMessageContext(), file_system_type,
                     GetFileSystemContextFromRenderId(render_process_id)));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileSystemBrowserHost::OnHostMsgInitIsolatedFileSystem(
    ppapi::host::HostMessageContext* context,
    const std::string& fsid,
    PP_IsolatedFileSystemType_Private type) {
  // Do not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  // Do a sanity check.
  if (!storage::ValidateIsolatedFileSystemId(fsid))
    return PP_ERROR_BADARGUMENT;

  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderFrameIDsForInstance(
          pp_instance(), &render_process_id, &unused)) {
    storage::IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
    return PP_ERROR_FAILED;
  }

  GURL origin = browser_ppapi_host_->GetDocumentURLForInstance(pp_instance())
                    .DeprecatedGetOriginAsURL();
  GURL root_url = GURL(storage::GetIsolatedFileSystemRootURIString(
      origin, fsid, ppapi::IsolatedFileSystemTypeToRootName(type)));

  const std::string& plugin_id = GeneratePluginId(GetPluginMimeType());

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IOThreadState::OpenIsolatedFileSystem, io_thread_state_,
                     origin, root_url, plugin_id,
                     context->MakeReplyMessageContext(), fsid, type,
                     GetFileSystemContextFromRenderId(render_process_id)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileSystemBrowserHost::OnHostMsgReserveQuota(
    ppapi::host::HostMessageContext* context,
    int64_t amount,
    const ppapi::FileGrowthMap& file_growths) {
  DCHECK(ChecksQuota());
  DCHECK_GT(amount, 0);

  if (reserving_quota_)
    return PP_ERROR_INPROGRESS;
  reserving_quota_ = true;

  int64_t reservation_amount =
      std::max<int64_t>(kMinimumQuotaReservationSize, amount);

  QuotaReservation::ReserveQuotaCallback callback = base::BindOnce(
      &PepperFileSystemBrowserHost::GotReservedQuota,
      weak_factory_.GetWeakPtr(), context->MakeReplyMessageContext());

  io_thread_state_->file_system_context()->default_file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &QuotaReservation::ReserveQuota, quota_reservation_,
          reservation_amount, file_growths,
          base::BindOnce(RunReserveQuotaCallbackOnUI, std::move(callback))));

  return PP_OK_COMPLETIONPENDING;
}

void PepperFileSystemBrowserHost::GotQuotaReservation(
    base::OnceClosure callback,
    scoped_refptr<QuotaReservation> quota_reservation) {
  quota_reservation_ = quota_reservation;
  std::move(callback).Run();
}

void PepperFileSystemBrowserHost::GotReservedQuota(
    ppapi::host::ReplyMessageContext reply_context,
    int64_t amount,
    const ppapi::FileSizeMap& file_sizes) {
  DCHECK(reserving_quota_);
  reserving_quota_ = false;
  reserved_quota_ = amount;

  reply_context.params.set_result(PP_OK);
  host()->SendReply(
      reply_context,
      PpapiPluginMsg_FileSystem_ReserveQuotaReply(amount, file_sizes));
}

std::string PepperFileSystemBrowserHost::GetPluginMimeType() const {
  base::FilePath plugin_path = browser_ppapi_host_->GetPluginPath();
  const ContentPluginInfo* info =
      PluginService::GetInstance()->GetRegisteredPluginInfo(plugin_path);
  if (!info || info->mime_types.empty())
    return std::string();
  // Use the first element in |info->mime_types| even if several elements exist.
  return info->mime_types[0].mime_type;
}

std::string PepperFileSystemBrowserHost::GeneratePluginId(
    const std::string& mime_type) const {
  // TODO(nhiroki): This function is very specialized for specific plugins (MIME
  // types).  If we bring this API to stable, we might have to make it more
  // general.

  std::string top_level_type;
  std::string subtype;
  if (!net::ParseMimeTypeWithoutParameter(
          mime_type, &top_level_type, &subtype) ||
      !net::IsValidTopLevelMimeType(top_level_type))
    return std::string();

  // Replace a slash used for type/subtype separator with an underscore.
  std::string output = top_level_type + "_" + subtype;

  // Verify |output| contains only alphabets, digits, or "._-".
  for (std::string::const_iterator it = output.begin(); it != output.end();
       ++it) {
    if (!base::IsAsciiAlpha(*it) && !base::IsAsciiDigit(*it) &&
        *it != '.' && *it != '_' && *it != '-') {
      LOG(WARNING) << "Failed to generate a plugin id.";
      return std::string();
    }
  }
  return output;
}

storage::FileSystemOperationRunner*
PepperFileSystemBrowserHost::GetFileSystemOperationRunnerInternal(
    scoped_refptr<IOThreadState> io_thread_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return io_thread_state->GetFileSystemOperationRunner();
}

}  // namespace content
