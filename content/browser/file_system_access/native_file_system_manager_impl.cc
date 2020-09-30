// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/native_file_system_manager_impl.h"

#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "content/browser/file_system_access/file_system_chooser.h"
#include "content/browser/file_system_access/fixed_native_file_system_permission_grant.h"
#include "content/browser/file_system_access/native_file_system.pb.h"
#include "content/browser/file_system_access/native_file_system_directory_handle_impl.h"
#include "content/browser/file_system_access/native_file_system_drag_drop_token_impl.h"
#include "content/browser/file_system_access/native_file_system_error.h"
#include "content/browser/file_system_access/native_file_system_file_handle_impl.h"
#include "content/browser/file_system_access/native_file_system_file_writer_impl.h"
#include "content/browser/file_system_access/native_file_system_transfer_token_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_drag_drop_token.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_error.mojom.h"
#include "url/origin.h"

namespace content {

using blink::mojom::NativeFileSystemStatus;
using PermissionStatus = NativeFileSystemPermissionGrant::PermissionStatus;
using SensitiveDirectoryResult =
    NativeFileSystemPermissionContext::SensitiveDirectoryResult;
using storage::FileSystemContext;
using HandleType = NativeFileSystemPermissionContext::HandleType;

namespace {

void ShowFilePickerOnUIThread(const url::Origin& requesting_origin,
                              GlobalFrameRoutingId frame_id,
                              const FileSystemChooser::Options& options,
                              FileSystemChooser::ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh = RenderFrameHost::FromID(frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);

  if (!web_contents) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kOperationAborted),
                            {});
    return;
  }

  url::Origin embedding_origin =
      url::Origin::Create(web_contents->GetLastCommittedURL());
  if (embedding_origin != requesting_origin) {
    // Third party iframes are not allowed to show a file picker.
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied,
            "Third party iframes are not allowed to show a file picker."),
        {});
    return;
  }

  // Drop fullscreen mode so that the user sees the URL bar.
  base::ScopedClosureRunner fullscreen_block =
      web_contents->ForSecurityDropFullscreen();

  FileSystemChooser::CreateAndShow(web_contents, options, std::move(callback),
                                   std::move(fullscreen_block));
}

// Called after creating a file that was picked by a save file picker. If
// creation succeeded (or the file already existed) this will attempt to
// truncate the file to zero bytes, and call `callback` on `reply_runner`
// with the result of this operation.
void DidCreateFileToTruncate(
    storage::FileSystemURL url,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> reply_runner,
    storage::FileSystemOperationRunner* operation_runner,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    // Failed to create the file, don't even try to truncate it.
    reply_runner->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }
  operation_runner->Truncate(
      url, /*length=*/0,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             scoped_refptr<base::SequencedTaskRunner> reply_runner,
             base::File::Error result) {
            reply_runner->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback),
                                          result == base::File::FILE_OK));
          },
          std::move(callback), std::move(reply_runner)));
}

// Creates and truncates the file at `url`. Calls `callback` on `reply_runner`
// with true if this succeeded, or false if either creation or truncation
// failed.
void CreateAndTruncateFile(
    storage::FileSystemURL url,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> reply_runner,
    storage::FileSystemOperationRunner* operation_runner) {
  // Binding operation_runner as a raw pointer is safe, since the callback is
  // invoked by the operation runner, and thus won't be invoked if the operation
  // runner has been destroyed.
  operation_runner->CreateFile(
      url, /*exclusive=*/false,
      base::BindOnce(&DidCreateFileToTruncate, url, std::move(callback),
                     std::move(reply_runner), operation_runner));
}

bool IsValidTransferToken(NativeFileSystemTransferTokenImpl* token,
                          const url::Origin& expected_origin,
                          HandleType expected_handle_type) {
  if (!token) {
    return false;
  }

  if (token->type() != expected_handle_type) {
    return false;
  }

  if (token->origin() != expected_origin) {
    return false;
  }

  return true;
}

HandleType GetFileType(const base::FilePath& file_path) {
  base::File::Info file_info;
  base::GetFileInfo(file_path, &file_info);
  return file_info.is_directory ? HandleType::kDirectory : HandleType::kFile;
}

}  // namespace

NativeFileSystemManagerImpl::SharedHandleState::SharedHandleState(
    scoped_refptr<NativeFileSystemPermissionGrant> read_grant,
    scoped_refptr<NativeFileSystemPermissionGrant> write_grant,
    storage::IsolatedContext::ScopedFSHandle file_system)
    : read_grant(std::move(read_grant)),
      write_grant(std::move(write_grant)),
      file_system(std::move(file_system)) {
  DCHECK(this->read_grant);
  DCHECK(this->write_grant);
}

NativeFileSystemManagerImpl::SharedHandleState::SharedHandleState(
    const SharedHandleState& other) = default;
NativeFileSystemManagerImpl::SharedHandleState::~SharedHandleState() = default;

NativeFileSystemManagerImpl::NativeFileSystemManagerImpl(
    scoped_refptr<storage::FileSystemContext> context,
    scoped_refptr<ChromeBlobStorageContext> blob_context,
    NativeFileSystemPermissionContext* permission_context,
    bool off_the_record)
    : context_(std::move(context)),
      blob_context_(std::move(blob_context)),
      permission_context_(permission_context),
      off_the_record_(off_the_record) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  DCHECK(blob_context_);
}

NativeFileSystemManagerImpl::~NativeFileSystemManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeFileSystemManagerImpl::BindReceiver(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network::IsOriginPotentiallyTrustworthy(binding_context.origin)) {
    mojo::ReportBadMessage("Native File System access from Unsecure Origin");
    return;
  }

  receivers_.Add(this, std::move(receiver), binding_context);
}

void NativeFileSystemManagerImpl::BindInternalsReceiver(
    mojo::PendingReceiver<storage::mojom::NativeFileSystemContext> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  internals_receivers_.Add(this, std::move(receiver));
}

void NativeFileSystemManagerImpl::GetSandboxedFileSystem(
    GetSandboxedFileSystemCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto response_callback = base::BindOnce(
      [](base::WeakPtr<NativeFileSystemManagerImpl> manager,
         const BindingContext& binding_context,
         GetSandboxedFileSystemCallback callback,
         scoped_refptr<base::SequencedTaskRunner> task_runner, const GURL& root,
         const std::string& fs_name, base::File::Error result) {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(
                &NativeFileSystemManagerImpl::DidOpenSandboxedFileSystem,
                std::move(manager), binding_context, std::move(callback), root,
                fs_name, result));
      },
      weak_factory_.GetWeakPtr(), receivers_.current_context(),
      std::move(callback), base::SequencedTaskRunnerHandle::Get());

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemContext::OpenFileSystem, context(),
                                receivers_.current_context().origin,
                                storage::kFileSystemTypeTemporary,
                                storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                std::move(response_callback)));
}

void NativeFileSystemManagerImpl::ChooseEntries(
    blink::mojom::ChooseFileSystemEntryType type,
    std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    bool include_accepts_all,
    ChooseEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const BindingContext& context = receivers_.current_context();

  // ChooseEntries API is only available to windows, as we need a frame to
  // anchor the picker to.
  if (context.is_worker()) {
    receivers_.ReportBadMessage("ChooseEntries called from a worker");
    return;
  }

  if (permission_context_) {
    // When site setting is block, it's better not to show file chooser.
    if (!permission_context_->CanObtainReadPermission(context.origin) ||
        (type == blink::mojom::ChooseFileSystemEntryType::kSaveFile &&
         !permission_context_->CanObtainWritePermission(context.origin))) {
      std::move(callback).Run(
          native_file_system_error::FromStatus(
              NativeFileSystemStatus::kPermissionDenied),
          std::vector<blink::mojom::NativeFileSystemEntryPtr>());
      return;
    }
  }

  RenderFrameHost* rfh = RenderFrameHost::FromID(context.frame_id);
  if (!rfh) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kOperationAborted),
        std::vector<blink::mojom::NativeFileSystemEntryPtr>());
    return;
  }

  // Renderer process should already check for user activation before sending
  // IPC, but just to be sure double check here as well. This is not treated
  // as a BadMessage because it is possible for the transient user activation
  // to expire between the renderer side check and this check.
  if (!rfh->HasTransientUserActivation()) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied,
            "User activation is required to show a file picker."),
        std::vector<blink::mojom::NativeFileSystemEntryPtr>());
    return;
  }

  FileSystemChooser::Options options(type, std::move(accepts),
                                     include_accepts_all);
  ShowFilePickerOnUIThread(
      context.origin, context.frame_id, options,
      base::BindOnce(&NativeFileSystemManagerImpl::DidChooseEntries,
                     weak_factory_.GetWeakPtr(), context, options,
                     std::move(callback)));
}

void NativeFileSystemManagerImpl::CreateNativeFileSystemDragDropToken(
    const base::FilePath& file_path,
    int renderer_id,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemDragDropToken>
        receiver) {
  auto drag_drop_token_impl =
      std::make_unique<NativeFileSystemDragDropTokenImpl>(
          this, file_path, renderer_id, std::move(receiver));
  auto token = drag_drop_token_impl->token();
  drag_drop_tokens_.emplace(token, std::move(drag_drop_token_impl));
}

void NativeFileSystemManagerImpl::GetEntryFromDragDropToken(
    mojo::PendingRemote<blink::mojom::NativeFileSystemDragDropToken> token,
    GetEntryFromDragDropTokenCallback token_resolved_callback) {
  mojo::Remote<blink::mojom::NativeFileSystemDragDropToken> drop_token_remote(
      std::move(token));

  // Get a failure callback in case this token ends up not being valid (i.e.
  // unrecognized token or wrong renderer process ID).
  mojo::ReportBadMessageCallback failed_token_redemption_callback =
      receivers_.GetBadMessageCallback();

  // Must pass `drop_token_remote` into GetInternalId in order to ensure it
  // stays in scope long enough for the callback to be called.
  auto* raw_drop_token_remote = drop_token_remote.get();
  raw_drop_token_remote->GetInternalId(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &NativeFileSystemManagerImpl::ResolveDragDropToken,
              weak_factory_.GetWeakPtr(), std::move(drop_token_remote),
              receivers_.current_context(), std::move(token_resolved_callback),
              std::move(failed_token_redemption_callback)),
          base::UnguessableToken()));
}

void NativeFileSystemManagerImpl::ResolveDragDropToken(
    mojo::Remote<blink::mojom::NativeFileSystemDragDropToken>,
    const BindingContext& binding_context,
    GetEntryFromDragDropTokenCallback token_resolved_callback,
    mojo::ReportBadMessageCallback failed_token_redemption_callback,
    const base::UnguessableToken& token) {
  auto drag_token_impl = drag_drop_tokens_.find(token);

  // Call `token_resolved_callback` with an error if the token isn't registered.
  if (drag_token_impl == drag_drop_tokens_.end()) {
    std::move(failed_token_redemption_callback)
        .Run("Unrecognized drag drop token.");
    return;
  }

  // Call `token_resolved_callback` with an error if the process redeeming the
  // token isn't the same process that the token is registered to.
  if (drag_token_impl->second->renderer_process_id() !=
      binding_context.process_id()) {
    std::move(failed_token_redemption_callback).Run("Invalid renderer ID.");
    return;
  }

  // Look up whether the file path that's associated with the token is a file or
  // directory and call ResolveDragDropTokenWithFileType with the result.
  const base::FilePath& drag_drop_token_path =
      drag_token_impl->second->file_path();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GetFileType, drag_drop_token_path),
      base::BindOnce(
          &NativeFileSystemManagerImpl::ResolveDragDropTokenWithFileType,
          weak_factory_.GetWeakPtr(), binding_context, drag_drop_token_path,
          std::move(token_resolved_callback)));
}

void NativeFileSystemManagerImpl::ResolveDragDropTokenWithFileType(
    const BindingContext& binding_context,
    const base::FilePath& file_path,
    GetEntryFromDragDropTokenCallback token_resolved_callback,
    HandleType file_type) {
  blink::mojom::NativeFileSystemEntryPtr entry;
  // TODO(mek): Support Drag&Drop of non-local paths.
  if (file_type == HandleType::kDirectory) {
    entry = CreateDirectoryEntryFromPath(binding_context, PathType::kLocal,
                                         file_path, UserAction::kDragAndDrop);
  } else {
    entry = CreateFileEntryFromPath(binding_context, PathType::kLocal,
                                    file_path, UserAction::kDragAndDrop);
  }

  std::move(token_resolved_callback).Run(std::move(entry));
}

void NativeFileSystemManagerImpl::GetFileHandleFromToken(
    mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemFileHandle>
        file_handle_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResolveTransferToken(
      std::move(token),
      base::BindOnce(
          &NativeFileSystemManagerImpl::DidResolveTransferTokenForFileHandle,
          weak_factory_.GetWeakPtr(), receivers_.current_context(),
          std::move(file_handle_receiver)));
}

void NativeFileSystemManagerImpl::GetDirectoryHandleFromToken(
    mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemDirectoryHandle>
        directory_handle_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResolveTransferToken(
      std::move(token),
      base::BindOnce(&NativeFileSystemManagerImpl::
                         DidResolveTransferTokenForDirectoryHandle,
                     weak_factory_.GetWeakPtr(), receivers_.current_context(),
                     std::move(directory_handle_receiver)));
}

void NativeFileSystemManagerImpl::SerializeHandle(
    mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
    SerializeHandleCallback callback) {
  ResolveTransferToken(
      std::move(token),
      base::BindOnce(&NativeFileSystemManagerImpl::DidResolveForSerializeHandle,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

namespace {

std::string SerializePath(const base::FilePath& path) {
  auto path_bytes = base::as_bytes(base::make_span(path.value()));
  return std::string(path_bytes.begin(), path_bytes.end());
}

base::FilePath DeserializePath(const std::string& bytes) {
  base::FilePath::StringType s;
  s.resize(bytes.size() / sizeof(base::FilePath::CharType));
  std::memcpy(&s[0], bytes.data(), s.size() * sizeof(base::FilePath::CharType));
  return base::FilePath(s);
}

}  // namespace

void NativeFileSystemManagerImpl::DidResolveForSerializeHandle(
    SerializeHandleCallback callback,
    NativeFileSystemTransferTokenImpl* resolved_token) {
  if (!resolved_token) {
    std::move(callback).Run({});
    return;
  }

  const storage::FileSystemURL& url = resolved_token->url();

  NativeFileSystemHandleData data;
  data.set_handle_type(resolved_token->type() == HandleType::kFile
                           ? NativeFileSystemHandleData::kFile
                           : NativeFileSystemHandleData::kDirectory);

  if (url.type() == storage::kFileSystemTypeNativeLocal ||
      url.mount_type() == storage::kFileSystemTypeExternal) {
    // A url can have mount_type = external and type = native local at the same
    // time. In that case we want to still treat it as an external path.
    const bool is_external =
        url.mount_type() == storage::kFileSystemTypeExternal;
    content::NativeFileData* file_data =
        is_external ? data.mutable_external() : data.mutable_local();

    base::FilePath url_path = is_external ? url.virtual_path() : url.path();
    base::FilePath root_path = resolved_token->GetWriteGrant()->GetPath();
    if (root_path.empty())
      root_path = url_path;

    file_data->set_root_path(SerializePath(root_path));

    base::FilePath relative_path;
    // We want |relative_path| to be the path of the file or directory
    // relative to |root_path|. FilePath::AppendRelativePath gets us that,
    // but fails if the path we're looking for is equal to the |root_path|.
    // So special case that case (in which case relative path would be empty
    // anyway).
    if (root_path != url_path) {
      bool relative_path_result =
          root_path.AppendRelativePath(url_path, &relative_path);
      DCHECK(relative_path_result);
    }

    file_data->set_relative_path(SerializePath(relative_path));
  } else if (url.type() == storage::kFileSystemTypeTemporary) {
    base::FilePath virtual_path = url.virtual_path();
    data.mutable_sandboxed()->set_virtual_path(SerializePath(virtual_path));

  } else {
    NOTREACHED();
  }

  std::string value;
  bool success = data.SerializeToString(&value);
  DCHECK(success);
  std::vector<uint8_t> result(value.begin(), value.end());
  std::move(callback).Run(result);
}

void NativeFileSystemManagerImpl::DeserializeHandle(
    const url::Origin& origin,
    const std::vector<uint8_t>& bits,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> token) {
  DCHECK(!bits.empty());

  std::string bits_as_string(bits.begin(), bits.end());
  NativeFileSystemHandleData data;
  if (!data.ParseFromString(bits_as_string)) {
    // Drop |token|, and directly return.
    return;
  }

  switch (data.data_case()) {
    case NativeFileSystemHandleData::kSandboxed: {
      base::FilePath virtual_path =
          DeserializePath(data.sandboxed().virtual_path());
      storage::FileSystemURL url = context()->CreateCrackedFileSystemURL(
          origin, storage::kFileSystemTypeTemporary, virtual_path);

      auto permission_grant =
          base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
              PermissionStatus::GRANTED, base::FilePath());
      CreateTransferTokenImpl(
          url, origin,
          SharedHandleState(permission_grant, permission_grant, {}),
          data.handle_type() == NativeFileSystemHandleData::kDirectory
              ? HandleType::kDirectory
              : HandleType::kFile,
          std::move(token));
      break;
    }
    case NativeFileSystemHandleData::kLocal:
    case NativeFileSystemHandleData::kExternal: {
      const content::NativeFileData& file_data =
          data.data_case() == NativeFileSystemHandleData::kLocal
              ? data.local()
              : data.external();

      base::FilePath root_path = DeserializePath(file_data.root_path());
      base::FilePath relative_path = DeserializePath(file_data.relative_path());
      FileSystemURLAndFSHandle root = CreateFileSystemURLFromPath(
          origin,
          data.data_case() == NativeFileSystemHandleData::kLocal
              ? PathType::kLocal
              : PathType::kExternal,
          root_path);

      storage::FileSystemURL child = context()->CreateCrackedFileSystemURL(
          root.url.origin(), root.url.mount_type(),
          root.url.virtual_path().Append(relative_path));

      const bool is_directory =
          data.handle_type() == NativeFileSystemHandleData::kDirectory;

      // Permissions are scoped to |root_path|, rather than the individual
      // handle. So if |relative_path| is not empty, this creates a
      // SharedHandleState for a directory even if the handle represents a
      // file.
      SharedHandleState handle_state = GetSharedHandleStateForPath(
          root_path, origin, std::move(root.file_system),
          (is_directory || !relative_path.empty()) ? HandleType::kDirectory
                                                   : HandleType::kFile,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage);

      CreateTransferTokenImpl(
          child, origin, handle_state,
          is_directory ? HandleType::kDirectory : HandleType::kFile,
          std::move(token));
      break;
    }
    case NativeFileSystemHandleData::DATA_NOT_SET:
      NOTREACHED();
  }
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateFileEntryFromPath(
    const BindingContext& binding_context,
    PathType path_type,
    const base::FilePath& file_path,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FileSystemURLAndFSHandle url =
      CreateFileSystemURLFromPath(binding_context.origin, path_type, file_path);

  SharedHandleState shared_handle_state = GetSharedHandleStateForPath(
      file_path, binding_context.origin, std::move(url.file_system),
      HandleType::kFile, user_action);

  return blink::mojom::NativeFileSystemEntry::New(
      blink::mojom::NativeFileSystemHandle::NewFile(
          CreateFileHandle(binding_context, url.url, shared_handle_state)),
      url.base_name);
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateDirectoryEntryFromPath(
    const BindingContext& binding_context,
    PathType path_type,
    const base::FilePath& file_path,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FileSystemURLAndFSHandle url =
      CreateFileSystemURLFromPath(binding_context.origin, path_type, file_path);

  SharedHandleState shared_handle_state = GetSharedHandleStateForPath(
      file_path, binding_context.origin, std::move(url.file_system),
      HandleType::kDirectory, user_action);

  return blink::mojom::NativeFileSystemEntry::New(
      blink::mojom::NativeFileSystemHandle::NewDirectory(
          CreateDirectoryHandle(binding_context, url.url, shared_handle_state)),
      url.base_name);
}

mojo::PendingRemote<blink::mojom::NativeFileSystemFileHandle>
NativeFileSystemManagerImpl::CreateFileHandle(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());
  DCHECK_EQ(url.mount_type() == storage::kFileSystemTypeIsolated,
            handle_state.file_system.is_valid())
      << url.mount_type();

  mojo::PendingRemote<blink::mojom::NativeFileSystemFileHandle> result;
  file_receivers_.Add(std::make_unique<NativeFileSystemFileHandleImpl>(
                          this, binding_context, url, handle_state),
                      result.InitWithNewPipeAndPassReceiver());
  return result;
}

mojo::PendingRemote<blink::mojom::NativeFileSystemDirectoryHandle>
NativeFileSystemManagerImpl::CreateDirectoryHandle(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());
  DCHECK_EQ(url.mount_type() == storage::kFileSystemTypeIsolated,
            handle_state.file_system.is_valid())
      << url.mount_type();

  mojo::PendingRemote<blink::mojom::NativeFileSystemDirectoryHandle> result;
  directory_receivers_.Add(
      std::make_unique<NativeFileSystemDirectoryHandleImpl>(
          this, binding_context, url, handle_state),
      result.InitWithNewPipeAndPassReceiver());
  return result;
}

mojo::PendingRemote<blink::mojom::NativeFileSystemFileWriter>
NativeFileSystemManagerImpl::CreateFileWriter(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    const SharedHandleState& handle_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<blink::mojom::NativeFileSystemFileWriter> result;

  RenderFrameHost* rfh = RenderFrameHost::FromID(binding_context.frame_id);
  bool has_transient_user_activation = rfh && rfh->HasTransientUserActivation();
  writer_receivers_.Add(
      std::make_unique<NativeFileSystemFileWriterImpl>(
          this, binding_context, url, swap_url, handle_state,
          has_transient_user_activation,
          GetContentClient()->browser()->GetQuarantineConnectionCallback()),
      result.InitWithNewPipeAndPassReceiver());
  return result;
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemFileHandleImpl& file,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  return CreateTransferTokenImpl(file.url(), file.context().origin,
                                 file.handle_state(), HandleType::kFile,
                                 std::move(receiver));
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemDirectoryHandleImpl& directory,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  return CreateTransferTokenImpl(directory.url(), directory.context().origin,
                                 directory.handle_state(),
                                 HandleType::kDirectory, std::move(receiver));
}

void NativeFileSystemManagerImpl::ResolveTransferToken(
    mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
    ResolvedTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<blink::mojom::NativeFileSystemTransferToken> token_remote(
      std::move(token));
  auto* raw_token = token_remote.get();
  raw_token->GetInternalID(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&NativeFileSystemManagerImpl::DoResolveTransferToken,
                     weak_factory_.GetWeakPtr(), std::move(token_remote),
                     std::move(callback)),
      base::UnguessableToken()));
}

void NativeFileSystemManagerImpl::DidResolveTransferTokenForFileHandle(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemFileHandle>
        file_handle_receiver,
    NativeFileSystemTransferTokenImpl* resolved_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidTransferToken(resolved_token, binding_context.origin,
                            HandleType::kFile)) {
    // Fail silently. In practice, the NativeFileSystemManager should not
    // receive any invalid tokens. Before redeeming a token, the render process
    // performs an origin check to ensure the token is valid. Invalid tokens
    // indicate a code bug or a compromised render process.
    //
    // After receiving an invalid token, the NativeFileSystemManager
    // cannot determine which render process is compromised. Is it the post
    // message sender or receiver? Because of this, the NativeFileSystemManager
    // closes the FileHandle pipe and ignores the error.
    return;
  }

  file_receivers_.Add(resolved_token->CreateFileHandle(binding_context),
                      std::move(file_handle_receiver));
}

void NativeFileSystemManagerImpl::DidResolveTransferTokenForDirectoryHandle(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemDirectoryHandle>
        directory_handle_receiver,
    NativeFileSystemTransferTokenImpl* resolved_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidTransferToken(resolved_token, binding_context.origin,
                            HandleType::kDirectory)) {
    // Fail silently. See comment above in
    // DidResolveTransferTokenForFileHandle() for details.
    return;
  }

  directory_receivers_.Add(
      resolved_token->CreateDirectoryHandle(binding_context),
      std::move(directory_handle_receiver));
}

const base::SequenceBound<storage::FileSystemOperationRunner>&
NativeFileSystemManagerImpl::operation_runner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!operation_runner_) {
    operation_runner_ =
        context()->CreateSequenceBoundFileSystemOperationRunner();
  }
  return operation_runner_;
}

void NativeFileSystemManagerImpl::DidOpenSandboxedFileSystem(
    const BindingContext& binding_context,
    GetSandboxedFileSystemCallback callback,
    const GURL& root,
    const std::string& filesystem_name,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(native_file_system_error::FromFileError(result),
                            mojo::NullRemote());
    return;
  }

  auto permission_grant =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          PermissionStatus::GRANTED, base::FilePath());

  std::move(callback).Run(
      native_file_system_error::Ok(),
      CreateDirectoryHandle(
          binding_context, context()->CrackURL(root),
          SharedHandleState(permission_grant, permission_grant,
                            /*file_system=*/{})));
}

void NativeFileSystemManagerImpl::DidChooseEntries(
    const BindingContext& binding_context,
    const FileSystemChooser::Options& options,
    ChooseEntriesCallback callback,
    blink::mojom::NativeFileSystemErrorPtr result,
    std::vector<FileSystemChooser::ResultEntry> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result->status != NativeFileSystemStatus::kOk) {
    std::move(callback).Run(
        std::move(result),
        std::vector<blink::mojom::NativeFileSystemEntryPtr>());
    return;
  }

  if (!permission_context_) {
    DidVerifySensitiveDirectoryAccess(binding_context, options,
                                      std::move(callback), std::move(entries),
                                      SensitiveDirectoryResult::kAllowed);
    return;
  }

  std::vector<base::FilePath> paths;
  paths.reserve(entries.size());
  for (const auto& entry : entries)
    paths.push_back(entry.path);

  const bool is_directory =
      options.type() == blink::mojom::ChooseFileSystemEntryType::kOpenDirectory;
  permission_context_->ConfirmSensitiveDirectoryAccess(
      binding_context.origin, std::move(paths),
      is_directory ? HandleType::kDirectory : HandleType::kFile,
      binding_context.frame_id,
      base::BindOnce(
          &NativeFileSystemManagerImpl::DidVerifySensitiveDirectoryAccess,
          weak_factory_.GetWeakPtr(), binding_context, options,
          std::move(callback), std::move(entries)));
}

void NativeFileSystemManagerImpl::DidVerifySensitiveDirectoryAccess(
    const BindingContext& binding_context,
    const FileSystemChooser::Options& options,
    ChooseEntriesCallback callback,
    std::vector<FileSystemChooser::ResultEntry> entries,
    SensitiveDirectoryResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramEnumeration(
      "NativeFileSystemAPI.SensitiveDirectoryAccessResult", result);

  if (result == SensitiveDirectoryResult::kAbort) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kOperationAborted),
        std::vector<blink::mojom::NativeFileSystemEntryPtr>());
    return;
  }
  if (result == SensitiveDirectoryResult::kTryAgain) {
    ShowFilePickerOnUIThread(
        binding_context.origin, binding_context.frame_id, options,
        base::BindOnce(&NativeFileSystemManagerImpl::DidChooseEntries,
                       weak_factory_.GetWeakPtr(), binding_context, options,
                       std::move(callback)));
    return;
  }

  if (options.type() ==
      blink::mojom::ChooseFileSystemEntryType::kOpenDirectory) {
    DCHECK_EQ(entries.size(), 1u);
    SharedHandleState shared_handle_state = GetSharedHandleStateForPath(
        entries.front().path, binding_context.origin, {},
        HandleType::kDirectory,
        NativeFileSystemPermissionContext::UserAction::kOpen);
    shared_handle_state.read_grant->RequestPermission(
        binding_context.frame_id,
        NativeFileSystemPermissionGrant::UserActivationState::kNotRequired,
        base::BindOnce(&NativeFileSystemManagerImpl::DidChooseDirectory, this,
                       binding_context, entries.front(), std::move(callback),
                       shared_handle_state));
    return;
  }

  if (options.type() == blink::mojom::ChooseFileSystemEntryType::kSaveFile) {
    DCHECK_EQ(entries.size(), 1u);
    // Create file if it doesn't yet exist, and truncate file if it does exist.
    FileSystemURLAndFSHandle url = CreateFileSystemURLFromPath(
        binding_context.origin, entries.front().type, entries.front().path);

    auto fs_url = url.url;
    operation_runner().PostTaskWithThisObject(
        FROM_HERE,
        base::BindOnce(
            &CreateAndTruncateFile, fs_url,
            base::BindOnce(
                &NativeFileSystemManagerImpl::DidCreateAndTruncateSaveFile,
                this, binding_context, entries.front(), std::move(url),
                std::move(callback)),
            base::SequencedTaskRunnerHandle::Get()));
    return;
  }

  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  result_entries.reserve(entries.size());
  for (const auto& entry : entries) {
    result_entries.push_back(CreateFileEntryFromPath(
        binding_context, entry.type, entry.path, UserAction::kOpen));
  }
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::DidCreateAndTruncateSaveFile(
    const BindingContext& binding_context,
    const FileSystemChooser::ResultEntry& entry,
    FileSystemURLAndFSHandle url,
    ChooseEntriesCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  if (!success) {
    // TODO(https://crbug.com/1124871): Failure to create or truncate the file
    // should probably not just result in a generic error, but instead inform
    // the user of the problem?
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            blink::mojom::NativeFileSystemStatus::kOperationFailed,
            "Failed to create or truncate file"),
        std::move(result_entries));
    return;
  }

  SharedHandleState shared_handle_state = GetSharedHandleStateForPath(
      entry.path, binding_context.origin, std::move(url.file_system),
      HandleType::kFile, UserAction::kSave);

  result_entries.push_back(blink::mojom::NativeFileSystemEntry::New(
      blink::mojom::NativeFileSystemHandle::NewFile(
          CreateFileHandle(binding_context, url.url, shared_handle_state)),
      url.base_name));

  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::DidChooseDirectory(
    const BindingContext& binding_context,
    const FileSystemChooser::ResultEntry& entry,
    ChooseEntriesCallback callback,
    const SharedHandleState& shared_handle_state,
    NativeFileSystemPermissionGrant::PermissionRequestOutcome outcome) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramEnumeration(
      "NativeFileSystemAPI.ConfirmReadDirectoryResult",
      shared_handle_state.read_grant->GetStatus());

  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  if (shared_handle_state.read_grant->GetStatus() !=
      PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kOperationAborted),
                            std::move(result_entries));
    return;
  }

  FileSystemURLAndFSHandle url = CreateFileSystemURLFromPath(
      binding_context.origin, entry.type, entry.path);

  result_entries.push_back(blink::mojom::NativeFileSystemEntry::New(
      blink::mojom::NativeFileSystemHandle::NewDirectory(CreateDirectoryHandle(
          binding_context, url.url,
          SharedHandleState(shared_handle_state.read_grant,
                            shared_handle_state.write_grant,
                            std::move(url.file_system)))),
      url.base_name));
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::CreateTransferTokenImpl(
    const storage::FileSystemURL& url,
    const url::Origin& origin,
    const SharedHandleState& handle_state,
    HandleType handle_type,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto token_impl = std::make_unique<NativeFileSystemTransferTokenImpl>(
      url, origin, handle_state, handle_type, this, std::move(receiver));
  auto token = token_impl->token();
  transfer_tokens_.emplace(token, std::move(token_impl));
}

void NativeFileSystemManagerImpl::RemoveToken(
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = transfer_tokens_.erase(token);
  DCHECK_EQ(1u, count_removed);
}

void NativeFileSystemManagerImpl::RemoveDragDropToken(
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = drag_drop_tokens_.erase(token);
  DCHECK_EQ(1u, count_removed);
}

void NativeFileSystemManagerImpl::DoResolveTransferToken(
    mojo::Remote<blink::mojom::NativeFileSystemTransferToken>,
    ResolvedTokenCallback callback,
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = transfer_tokens_.find(token);
  if (it == transfer_tokens_.end()) {
    std::move(callback).Run(nullptr);
  } else {
    std::move(callback).Run(it->second.get());
  }
}

NativeFileSystemManagerImpl::FileSystemURLAndFSHandle
NativeFileSystemManagerImpl::CreateFileSystemURLFromPath(
    const url::Origin& origin,
    PathType path_type,
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (path_type) {
    case PathType::kLocal: {
      auto* isolated_context = storage::IsolatedContext::GetInstance();
      DCHECK(isolated_context);

      FileSystemURLAndFSHandle result;
      result.file_system = isolated_context->RegisterFileSystemForPath(
          storage::kFileSystemTypeNativeLocal, std::string(), path,
          &result.base_name);

      base::FilePath root_path =
          isolated_context->CreateVirtualRootPath(result.file_system.id());
      // FromUTF8Unsafe in the following line is safe since result.base_name was
      // the result of calling AsUTF8Unsafe on a base::FilePath in the first
      // place.
      base::FilePath isolated_path =
          root_path.Append(base::FilePath::FromUTF8Unsafe(result.base_name));

      result.url = context()->CreateCrackedFileSystemURL(
          origin, storage::kFileSystemTypeIsolated, isolated_path);
      return result;
    }
    case PathType::kExternal: {
      FileSystemURLAndFSHandle result;
      result.url = context()->CreateCrackedFileSystemURL(
          url::Origin(), storage::kFileSystemTypeExternal, path);
      result.base_name = path.BaseName().AsUTF8Unsafe();
      return result;
    }
  }
}

NativeFileSystemManagerImpl::SharedHandleState
NativeFileSystemManagerImpl::GetSharedHandleStateForPath(
    const base::FilePath& path,
    const url::Origin& origin,
    storage::IsolatedContext::ScopedFSHandle file_system,
    HandleType handle_type,
    NativeFileSystemPermissionContext::UserAction user_action) {
  scoped_refptr<NativeFileSystemPermissionGrant> read_grant, write_grant;
  if (permission_context_) {
    read_grant = permission_context_->GetReadPermissionGrant(
        origin, path, handle_type, user_action);
    write_grant = permission_context_->GetWritePermissionGrant(
        origin, path, handle_type, user_action);
  } else {
    // Auto-deny all write grants if no permisson context is available, unless
    // Experimental Web Platform features are enabled.
    // TODO(mek): Remove experimental web platform check when permission UI is
    // implemented.
    write_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalWebPlatformFeatures)
            ? PermissionStatus::GRANTED
            : PermissionStatus::DENIED,
        path);
    if (user_action ==
        NativeFileSystemPermissionContext::UserAction::kLoadFromStorage) {
      read_grant = write_grant;
    } else {
      // Grant read permission even without a permission_context_, as the picker
      // itself is enough UI to assume user intent.
      read_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          PermissionStatus::GRANTED, path);
    }
  }
  return SharedHandleState(std::move(read_grant), std::move(write_grant),
                           file_system);
}

}  // namespace content
