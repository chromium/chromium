// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_manager_impl.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "content/browser/native_file_system/file_system_chooser.h"
#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"
#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_error.h"
#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_file_writer_impl.h"
#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"
#include "url/origin.h"

namespace content {

using blink::mojom::NativeFileSystemStatus;
using PermissionStatus = NativeFileSystemPermissionGrant::PermissionStatus;
using SensitiveDirectoryResult =
    NativeFileSystemPermissionContext::SensitiveDirectoryResult;
using storage::FileSystemContext;

namespace {

void ShowFilePickerOnUIThread(const url::Origin& requesting_origin,
                              int render_process_id,
                              int frame_id,
                              const FileSystemChooser::Options& options,
                              FileSystemChooser::ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh = RenderFrameHost::FromID(render_process_id, frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);

  if (!web_contents) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kOperationAborted),
                            std::vector<base::FilePath>());
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
        std::vector<base::FilePath>());
    return;
  }

  // Drop fullscreen mode so that the user sees the URL bar.
  web_contents->ForSecurityDropFullscreen();

  FileSystemChooser::CreateAndShow(web_contents, options, std::move(callback));
}

bool CreateOrTruncateFile(const base::FilePath& path) {
  int creation_flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
  base::File file(path, creation_flags);
  return file.IsValid();
}

bool IsValidTransferToken(
    NativeFileSystemTransferTokenImpl* token,
    const url::Origin& expected_origin,
    NativeFileSystemTransferTokenImpl::HandleType expected_handle_type) {
  if (!token) {
    return false;
  }

  if (token->type() != expected_handle_type) {
    return false;
  }

  if (token->url().origin() != expected_origin) {
    return false;
  }
  return true;
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
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network::IsOriginPotentiallyTrustworthy(binding_context.origin)) {
    mojo::ReportBadMessage("Native File System access from Unsecure Origin");
    return;
  }

  receivers_.Add(this, std::move(receiver), binding_context);
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

  GURL origin = receivers_.current_context().origin.GetURL();
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&FileSystemContext::OpenFileSystem, context(),
                                origin, storage::kFileSystemTypeTemporary,
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

  // When site setting is block, it's better not to show file chooser for save.
  if (type == blink::mojom::ChooseFileSystemEntryType::kSaveFile &&
      permission_context_ &&
      !permission_context_->CanRequestWritePermission(context.origin)) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied),
        std::vector<blink::mojom::NativeFileSystemEntryPtr>());

    return;
  }

  RenderFrameHost* rfh =
      RenderFrameHost::FromID(context.process_id, context.frame_id);
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
      context.origin, context.process_id, context.frame_id, options,
      base::BindOnce(&NativeFileSystemManagerImpl::DidChooseEntries,
                     weak_factory_.GetWeakPtr(), context, options,
                     std::move(callback)));
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

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateFileEntryFromPath(
    const BindingContext& binding_context,
    const base::FilePath& file_path) {
  return CreateFileEntryFromPathImpl(
      binding_context, file_path,
      NativeFileSystemPermissionContext::UserAction::kOpen);
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateDirectoryEntryFromPath(
    const BindingContext& binding_context,
    const base::FilePath& directory_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto url =
      CreateFileSystemURLFromPath(binding_context.origin, directory_path);

  scoped_refptr<NativeFileSystemPermissionGrant> read_grant, write_grant;
  if (permission_context_) {
    read_grant = permission_context_->GetReadPermissionGrant(
        binding_context.origin, directory_path, /*is_directory=*/true,
        binding_context.process_id, binding_context.frame_id);
    write_grant = permission_context_->GetWritePermissionGrant(
        binding_context.origin, directory_path, /*is_directory=*/true,
        binding_context.process_id, binding_context.frame_id,
        NativeFileSystemPermissionContext::UserAction::kOpen);
  } else {
    // Grant read permission even without a permission_context_, as the picker
    // itself is enough UI to assume user intent.
    read_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        PermissionStatus::GRANTED);
    // Auto-deny all write grants if no permisson context is available, unless
    // Experimental Web Platform features are enabled.
    // TODO(mek): Remove experimental web platform check when permission UI is
    // implemented.
    write_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalWebPlatformFeatures)
            ? PermissionStatus::GRANTED
            : PermissionStatus::DENIED);
  }

  return blink::mojom::NativeFileSystemEntry::New(
      blink::mojom::NativeFileSystemHandle::NewDirectory(CreateDirectoryHandle(
          binding_context, url.url,
          SharedHandleState(std::move(read_grant), std::move(write_grant),
                            std::move(url.file_system)))),
      url.base_name);
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateWritableFileEntryFromPath(
    const BindingContext& binding_context,
    const base::FilePath& file_path) {
  return CreateFileEntryFromPathImpl(
      binding_context, file_path,
      NativeFileSystemPermissionContext::UserAction::kSave);
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

  RenderFrameHost* rfh = RenderFrameHost::FromID(binding_context.process_id,
                                                 binding_context.frame_id);
  bool has_transient_user_activation = rfh && rfh->HasTransientUserActivation();
  writer_receivers_.Add(std::make_unique<NativeFileSystemFileWriterImpl>(
                            this, binding_context, url, swap_url, handle_state,
                            has_transient_user_activation),
                        result.InitWithNewPipeAndPassReceiver());
  return result;
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemFileHandleImpl& file,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  return CreateTransferTokenImpl(file.url(), file.handle_state(),
                                 /*is_directory=*/false, std::move(receiver));
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemDirectoryHandleImpl& directory,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  return CreateTransferTokenImpl(directory.url(), directory.handle_state(),
                                 /*is_directory=*/true, std::move(receiver));
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

  if (!IsValidTransferToken(
          resolved_token, binding_context.origin,
          NativeFileSystemTransferTokenImpl::HandleType::kFile)) {
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

  file_receivers_.Add(std::make_unique<NativeFileSystemFileHandleImpl>(
                          this, binding_context, resolved_token->url(),
                          resolved_token->shared_handle_state()),
                      std::move(file_handle_receiver));
}

void NativeFileSystemManagerImpl::DidResolveTransferTokenForDirectoryHandle(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemDirectoryHandle>
        directory_handle_receiver,
    NativeFileSystemTransferTokenImpl* resolved_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidTransferToken(
          resolved_token, binding_context.origin,
          NativeFileSystemTransferTokenImpl::HandleType::kDirectory)) {
    // Fail silently. See comment above in
    // DidResolveTransferTokenForFileHandle() for details.
    return;
  }

  directory_receivers_.Add(
      std::make_unique<NativeFileSystemDirectoryHandleImpl>(
          this, binding_context, resolved_token->url(),
          resolved_token->shared_handle_state()),
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
          PermissionStatus::GRANTED);

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
    std::vector<base::FilePath> entries) {
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
  auto entries_copy = entries;
  const bool is_directory =
      options.type() == blink::mojom::ChooseFileSystemEntryType::kOpenDirectory;
  permission_context_->ConfirmSensitiveDirectoryAccess(
      binding_context.origin, entries_copy, is_directory,
      binding_context.process_id, binding_context.frame_id,
      base::BindOnce(
          &NativeFileSystemManagerImpl::DidVerifySensitiveDirectoryAccess,
          weak_factory_.GetWeakPtr(), binding_context, options,
          std::move(callback), std::move(entries)));
}

void NativeFileSystemManagerImpl::DidVerifySensitiveDirectoryAccess(
    const BindingContext& binding_context,
    const FileSystemChooser::Options& options,
    ChooseEntriesCallback callback,
    std::vector<base::FilePath> entries,
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
        binding_context.origin, binding_context.process_id,
        binding_context.frame_id, options,
        base::BindOnce(&NativeFileSystemManagerImpl::DidChooseEntries,
                       weak_factory_.GetWeakPtr(), binding_context, options,
                       std::move(callback)));
    return;
  }

  if (options.type() ==
      blink::mojom::ChooseFileSystemEntryType::kOpenDirectory) {
    DCHECK_EQ(entries.size(), 1u);
    if (permission_context_) {
      permission_context_->ConfirmDirectoryReadAccess(
          binding_context.origin, entries.front(), binding_context.process_id,
          binding_context.frame_id,
          base::BindOnce(&NativeFileSystemManagerImpl::DidChooseDirectory, this,
                         binding_context, entries.front(),
                         std::move(callback)));
    } else {
      DidChooseDirectory(binding_context, entries.front(), std::move(callback),
                         PermissionStatus::GRANTED);
    }
    return;
  }

  if (options.type() == blink::mojom::ChooseFileSystemEntryType::kSaveFile) {
    DCHECK_EQ(entries.size(), 1u);
    // Create file if it doesn't yet exist, and truncate file if it does exist.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::TaskPriority::USER_BLOCKING,
         base::MayBlock()},
        base::BindOnce(&CreateOrTruncateFile, entries.front()),
        base::BindOnce(
            &NativeFileSystemManagerImpl::DidCreateOrTruncateSaveFile, this,
            binding_context, entries.front(), std::move(callback)));
    return;
  }

  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  result_entries.reserve(entries.size());
  for (const auto& entry : entries)
    result_entries.push_back(CreateFileEntryFromPath(binding_context, entry));
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::DidCreateOrTruncateSaveFile(
    const BindingContext& binding_context,
    const base::FilePath& path,
    ChooseEntriesCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  if (!success) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            blink::mojom::NativeFileSystemStatus::kOperationFailed,
            "Failed to create or truncate file"),
        std::move(result_entries));
    return;
  }
  result_entries.push_back(
      CreateWritableFileEntryFromPath(binding_context, path));
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::DidChooseDirectory(
    const BindingContext& binding_context,
    const base::FilePath& path,
    ChooseEntriesCallback callback,
    NativeFileSystemPermissionContext::PermissionStatus permission) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramEnumeration(
      "NativeFileSystemAPI.ConfirmReadDirectoryResult", permission);

  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  if (permission != PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kOperationAborted),
                            std::move(result_entries));
    return;
  }

  result_entries.push_back(CreateDirectoryEntryFromPath(binding_context, path));
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::CreateTransferTokenImpl(
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state,
    bool is_directory,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto token_impl = std::make_unique<NativeFileSystemTransferTokenImpl>(
      url, handle_state,
      is_directory ? NativeFileSystemTransferTokenImpl::HandleType::kDirectory
                   : NativeFileSystemTransferTokenImpl::HandleType::kFile,
      this, std::move(receiver));
  auto token = token_impl->token();
  transfer_tokens_.emplace(token, std::move(token_impl));
}

void NativeFileSystemManagerImpl::RemoveToken(
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = transfer_tokens_.erase(token);
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
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* isolated_context = storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  FileSystemURLAndFSHandle result;

  result.file_system = isolated_context->RegisterFileSystemForPath(
      storage::kFileSystemTypeNativeLocal, std::string(), path,
      &result.base_name);

  base::FilePath root_path =
      isolated_context->CreateVirtualRootPath(result.file_system.id());
  base::FilePath isolated_path = root_path.AppendASCII(result.base_name);

  result.url = context()->CreateCrackedFileSystemURL(
      origin.GetURL(), storage::kFileSystemTypeIsolated, isolated_path);
  return result;
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateFileEntryFromPathImpl(
    const BindingContext& binding_context,
    const base::FilePath& file_path,
    NativeFileSystemPermissionContext::UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto url = CreateFileSystemURLFromPath(binding_context.origin, file_path);

  scoped_refptr<NativeFileSystemPermissionGrant> read_grant, write_grant;
  if (permission_context_) {
    read_grant = permission_context_->GetReadPermissionGrant(
        binding_context.origin, file_path, /*is_directory=*/false,
        binding_context.process_id, binding_context.frame_id);
    write_grant = permission_context_->GetWritePermissionGrant(
        binding_context.origin, file_path, /*is_directory=*/false,
        binding_context.process_id, binding_context.frame_id, user_action);
  } else {
    // Grant read permission even without a permission_context_, as the picker
    // itself is enough UI to assume user intent.
    read_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        PermissionStatus::GRANTED);
    // Auto-deny all write grants if no permisson context is available, unless
    // Experimental Web Platform features are enabled.
    // TODO(mek): Remove experimental web platform check when permission UI is
    // implemented.
    write_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalWebPlatformFeatures)
            ? PermissionStatus::GRANTED
            : PermissionStatus::DENIED);
  }

  return blink::mojom::NativeFileSystemEntry::New(
      blink::mojom::NativeFileSystemHandle::NewFile(CreateFileHandle(
          binding_context, url.url,
          SharedHandleState(std::move(read_grant), std::move(write_grant),
                            std::move(url.file_system)))),
      url.base_name);
}

}  // namespace content
