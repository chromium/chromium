// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"

#include "base/files/file_error_or.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_access_handle_host_impl.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_handle_base.h"
#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/mime_util.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_modification_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "content/public/common/content_paths.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_MAC)
#include <sys/clonefile.h>

#include "base/files/file.h"
#endif

using blink::mojom::FileSystemAccessStatus;
using storage::BlobDataHandle;
using storage::BlobImpl;
using storage::FileSystemOperation;
using storage::FileSystemOperationRunner;

namespace content {

namespace {

std::pair<base::File, base::FileErrorOr<int64_t>> GetFileLengthOnBlockingThread(
    base::File file) {
  int64_t file_length = file.GetLength();
  if (file_length < 0) {
    return {std::move(file), base::unexpected(base::File::GetLastFileError())};
  }
  return {std::move(file), std::move(file_length)};
}

#if BUILDFLAG(IS_ANDROID)
void EnsureSwapDirExists(base::FilePath swap_dir) {
  if (!base::PathExists(swap_dir)) {
    if (!base::CreateDirectory(swap_dir)) {
      DLOG(ERROR) << "Error creating swap dir " << swap_dir;
    }
  }
}
#endif

bool HasWritePermission(const base::FilePath& path) {
  if (!base::PathExists(path)) {
    return true;
  }

#if BUILDFLAG(IS_POSIX)
  int mode;
  if (!base::GetPosixFilePermissions(path, &mode)) {
    return true;
  }

  if (!(mode & base::FILE_PERMISSION_WRITE_BY_USER)) {
    return false;
  }
#elif BUILDFLAG(IS_WIN)
  DWORD attrs = ::GetFileAttributes(path.value().c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return true;
  }
  if (attrs & FILE_ATTRIBUTE_READONLY) {
    return false;
  }
#endif  // BUILDFLAG(IS_POSIX)

  return true;
}

#if BUILDFLAG(IS_MAC)
// Creates a copy-on-write file at `swap_url`. This will fail if the file
// already exists. Must be called on a sequence which allows blocking.
base::File::Error CreateCowSwapFile(const storage::FileSystemURL& source_url,
                                    const storage::FileSystemURL& swap_url) {
  return clonefile(source_url.path().value().c_str(),
                   swap_url.path().value().c_str(),
                   /*flags=*/0) == 0
             ? base::File::Error::FILE_OK
             : base::File::GetLastFileError();
}
#endif  // BUILDFLAG(IS_MAC)

file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
CreateFileAccessCallback(const GURL& destination) {
  if (auto* file_access = file_access::ScopedFileAccessDelegate::Get()) {
    return file_access->CreateFileAccessCallback(destination);
  }
  return base::NullCallback();
}

}  // namespace

FileSystemAccessFileHandleImpl::FileSystemAccessFileHandleImpl(
    FileSystemAccessManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : FileSystemAccessHandleBase(manager, context, url, handle_state) {}

FileSystemAccessFileHandleImpl::~FileSystemAccessFileHandleImpl() = default;

void FileSystemAccessFileHandleImpl::GetPermissionStatus(
    bool writable,
    GetPermissionStatusCallback callback) {
  DoGetPermissionStatus(writable, std::move(callback));
}

void FileSystemAccessFileHandleImpl::RequestPermission(
    bool writable,
    RequestPermissionCallback callback) {
  DoRequestPermission(writable, std::move(callback));
}

void FileSystemAccessFileHandleImpl::AsBlob(AsBlobCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kPermissionDenied),
                            base::File::Info(), nullptr);
    return;
  }

  // TODO(mek): Check backend::SupportsStreaming and create snapshot file if
  // streaming is not supported.
  manager()->DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::GetMetadata,
      base::BindOnce(&FileSystemAccessFileHandleImpl::DidGetMetaDataForBlob,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      url(),
      FileSystemOperation::GetMetadataFieldSet(
          {FileSystemOperation::GetMetadataField::kIsDirectory,
           FileSystemOperation::GetMetadataField::kSize,
           FileSystemOperation::GetMetadataField::kLastModified}));
}

void FileSystemAccessFileHandleImpl::CreateFileWriter(
    bool keep_existing_data,
    bool auto_close,
    blink::mojom::FileSystemAccessWritableFileStreamLockMode mode,
    CreateFileWriterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessFileHandleImpl::CreateFileWriterImpl,
                     weak_factory_.GetWeakPtr(), keep_existing_data, auto_close,
                     mode),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        CreateFileWriterCallback callback) {
        std::move(callback).Run(std::move(result), mojo::NullRemote());
      }),
      std::move(callback));
}

void FileSystemAccessFileHandleImpl::Move(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        destination_directory,
    const std::string& new_entry_name,
    MoveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RenderFrameHost* rfh = RenderFrameHost::FromID(context().frame_id);
  bool has_transient_user_activation = rfh && rfh->HasTransientUserActivation();

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessHandleBase::DoMove,
                     weak_factory_.GetWeakPtr(),
                     std::move(destination_directory), new_entry_name,
                     has_transient_user_activation),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        MoveCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

void FileSystemAccessFileHandleImpl::Rename(const std::string& new_entry_name,
                                            RenameCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RenderFrameHost* rfh = RenderFrameHost::FromID(context().frame_id);
  bool has_transient_user_activation = rfh && rfh->HasTransientUserActivation();

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessHandleBase::DoRename,
                     weak_factory_.GetWeakPtr(), new_entry_name,
                     has_transient_user_activation),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        MoveCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

void FileSystemAccessFileHandleImpl::Remove(RemoveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessHandleBase::DoRemove,
                     weak_factory_.GetWeakPtr(), url(), /*recurse=*/false),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        RemoveCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

void FileSystemAccessFileHandleImpl::OpenAccessHandle(
    blink::mojom::FileSystemAccessAccessHandleLockMode mode,
    OpenAccessHandleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (url().type() != storage::kFileSystemTypeTemporary) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kInvalidState,
            "Access Handles may only be created on temporary file systems"),
        blink::mojom::FileSystemAccessAccessHandleFilePtr(),
        mojo::NullRemote());
    return;
  }

  FileSystemAccessLockManager::LockType lock_type;

  switch (mode) {
    case blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwrite:
      lock_type = manager()->GetExclusiveLockType();
      break;
    case blink::mojom::FileSystemAccessAccessHandleLockMode::kReadOnly:
      lock_type = manager()->GetSAHReadOnlyLockType();
      break;
    case blink::mojom::FileSystemAccessAccessHandleLockMode::kReadwriteUnsafe:
      lock_type = manager()->GetSAHReadwriteUnsafeLockType();
      break;
  }

  manager()->TakeLock(
      context(), url(), lock_type,
      base::BindOnce(&FileSystemAccessFileHandleImpl::DidTakeAccessHandleLock,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemAccessFileHandleImpl::DidTakeAccessHandleLock(
    OpenAccessHandleCallback callback,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!lock) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kNoModificationAllowedError,
            "Access Handles cannot be created if there is another open Access "
            "Handle or Writable stream associated with the same file."),
        blink::mojom::FileSystemAccessAccessHandleFilePtr(),
        mojo::NullRemote());
    return;
  }

  auto open_file_callback =
      file_system_context()->is_incognito()
          ? base::BindOnce(&FileSystemAccessFileHandleImpl::DoOpenIncognitoFile,
                           weak_factory_.GetWeakPtr(), std::move(lock))
          : base::BindOnce(&FileSystemAccessFileHandleImpl::DoOpenFile,
                           weak_factory_.GetWeakPtr(), std::move(lock));
  RunWithWritePermission(
      std::move(open_file_callback),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        OpenAccessHandleCallback callback) {
        std::move(callback).Run(
            std::move(result),
            blink::mojom::FileSystemAccessAccessHandleFilePtr(),
            mojo::NullRemote());
      }),
      std::move(callback));
}

void FileSystemAccessFileHandleImpl::DoOpenIncognitoFile(
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    OpenAccessHandleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  mojo::PendingRemote<blink::mojom::FileSystemAccessFileDelegateHost>
      file_delegate_host_remote;
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_host_remote = manager()->CreateAccessHandleHost(
          url(), file_delegate_host_remote.InitWithNewPipeAndPassReceiver(),
          mojo::NullReceiver(), 0, std::move(lock),
          base::ScopedClosureRunner());

  std::move(callback).Run(
      file_system_access_error::Ok(),
      blink::mojom::FileSystemAccessAccessHandleFile::NewIncognitoFileDelegate(
          std::move(file_delegate_host_remote)),
      std::move(access_handle_host_remote));
}

void FileSystemAccessFileHandleImpl::DoOpenFile(
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    OpenAccessHandleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  manager()->DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::OpenFile,
      base::BindOnce(&FileSystemAccessFileHandleImpl::DoGetLengthAfterOpenFile,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(lock)),
      url(),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
}

void FileSystemAccessFileHandleImpl::DoGetLengthAfterOpenFile(
    OpenAccessHandleCallback callback,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    base::File file,
    base::ScopedClosureRunner on_close_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::mojom::FileSystemAccessErrorPtr result =
      file_system_access_error::FromFileError(file.error_details());
  if (result->status != FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(result),
                            blink::mojom::FileSystemAccessAccessHandleFilePtr(),
                            mojo::NullRemote());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {
          // Needed for file I/O.
          base::MayBlock(),

          // Reasonable compromise, given that applications may block on file
          // operations.
          base::TaskPriority::USER_VISIBLE,

          // BLOCK_SHUTDOWN is definitely not appropriate. We might be able to
          // move to CONTINUE_ON_SHUTDOWN after very careful analysis.
          base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
      },
      base::BindOnce(&GetFileLengthOnBlockingThread, std::move(file)),
      base::BindOnce(&FileSystemAccessFileHandleImpl::DidOpenFileAndGetLength,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(lock), std::move(on_close_callback)));
}

void FileSystemAccessFileHandleImpl::DidOpenFileAndGetLength(
    OpenAccessHandleCallback callback,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    base::ScopedClosureRunner on_close_callback,
    std::pair<base::File, base::FileErrorOr<int64_t>> file_and_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::File file = std::move(file_and_length.first);
  base::FileErrorOr<int64_t> length_or_error =
      std::move(file_and_length.second);

  if (!length_or_error.has_value()) {
    std::move(callback).Run(
        file_system_access_error::FromFileError(length_or_error.error()),
        blink::mojom::FileSystemAccessAccessHandleFilePtr(),
        mojo::NullRemote());
    return;
  }
  DCHECK_GE(length_or_error.value(), 0);

  mojo::PendingRemote<blink::mojom::FileSystemAccessFileModificationHost>
      file_modification_host_remote;
  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
      access_handle_host_remote = manager()->CreateAccessHandleHost(
          url(), mojo::NullReceiver(),
          file_modification_host_remote.InitWithNewPipeAndPassReceiver(),
          length_or_error.value(), std::move(lock),
          std::move(on_close_callback));

  std::move(callback).Run(
      file_system_access_error::Ok(),
      blink::mojom::FileSystemAccessAccessHandleFile::NewRegularFile(
          blink::mojom::FileSystemAccessRegularFile::New(
              std::move(file), length_or_error.value(),
              std::move(file_modification_host_remote))),
      std::move(access_handle_host_remote));
}

void FileSystemAccessFileHandleImpl::IsSameEntry(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    IsSameEntryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->ResolveTransferToken(
      std::move(token),
      base::BindOnce(&FileSystemAccessFileHandleImpl::IsSameEntryImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemAccessFileHandleImpl::IsSameEntryImpl(
    IsSameEntryCallback callback,
    FileSystemAccessTransferTokenImpl* other) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!other) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed),
        false);
    return;
  }

  // The two handles are the same if they serialize to the same value.
  auto serialization = manager()->SerializeURL(
      url(), FileSystemAccessPermissionContext::HandleType::kFile);
  auto other_serialization =
      manager()->SerializeURL(other->url(), other->type());
  std::move(callback).Run(file_system_access_error::Ok(),
                          serialization == other_serialization);
}

void FileSystemAccessFileHandleImpl::Transfer(
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->CreateTransferToken(*this, std::move(token));
}

void FileSystemAccessFileHandleImpl::DidGetMetaDataForBlob(
    AsBlobCallback callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(file_system_access_error::FromFileError(result),
                            base::File::Info(), nullptr);
    return;
  }

  std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string content_type;

  base::FilePath::StringType extension = url().path().Extension();
  if (!extension.empty()) {
    std::string mime_type;
    // TODO(crbug.com/41458368): Using GetMimeTypeFromExtension and
    // including platform defined mime type mappings might be nice/make sense,
    // however that method can potentially block and thus can't be called from
    // the IO thread.
    if (net::GetWellKnownMimeTypeFromExtension(extension.substr(1),
                                               &mime_type)) {
      content_type = std::move(mime_type);
    }
  }
  // TODO(crbug.com/41458368): Consider some kind of fallback type when
  // the above mime type detection fails.

  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  mojo::PendingReceiver<blink::mojom::Blob> blob_receiver =
      blob_remote.InitWithNewPipeAndPassReceiver();

  std::move(callback).Run(
      file_system_access_error::Ok(), info,
      blink::mojom::SerializedBlob::New(uuid, content_type, info.size,
                                        std::move(blob_remote)));

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromeBlobStorageContext::CreateFileSystemBlobWithFileAccess,
          base::WrapRefCounted(manager()->blob_context()),
          base::WrapRefCounted(file_system_context()), std::move(blob_receiver),
          url(), std::move(uuid), std::move(content_type), info.size,
          info.last_modified, CreateFileAccessCallback(context().url)));
}

void FileSystemAccessFileHandleImpl::CreateFileWriterImpl(
    bool keep_existing_data,
    bool auto_close,
    blink::mojom::FileSystemAccessWritableFileStreamLockMode mode,
    CreateFileWriterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  // TODO(crbug.com/40194651): Expand this check to all backends.
  if (url().type() == storage::kFileSystemTypeLocal) {
    auto checks = base::BindOnce(&HasWritePermission, url().path());
#if BUILDFLAG(IS_ANDROID)
    if (url().path().IsContentUri()) {
      swap_dir_ =
          base::PathService::CheckedGet(content::DIR_FILE_SYSTEM_API_SWAP);
      checks = base::BindOnce(&EnsureSwapDirExists, swap_dir_)
                   .Then(std::move(checks));
    }
#endif
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()}, std::move(checks),
        base::BindOnce(
            &FileSystemAccessFileHandleImpl::DidVerifyHasWritePermissions,
            weak_factory_.GetWeakPtr(), keep_existing_data, auto_close, mode,
            std::move(callback)));
    return;
  }

  DidVerifyHasWritePermissions(keep_existing_data, auto_close, mode,
                               std::move(callback), /*can_write=*/true);
}

void FileSystemAccessFileHandleImpl::DidVerifyHasWritePermissions(
    bool keep_existing_data,
    bool auto_close,
    blink::mojom::FileSystemAccessWritableFileStreamLockMode mode,
    CreateFileWriterCallback callback,
    bool can_write) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!can_write) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kNoModificationAllowedError,
            "Cannot write to a read-only file."),
        mojo::NullRemote());
    return;
  }

  FileSystemAccessLockManager::LockType lock_type =
      mode == blink::mojom::FileSystemAccessWritableFileStreamLockMode::kSiloed
          ? manager()->GetWFSSiloedLockType()
          : manager()->GetExclusiveLockType();

  manager()->TakeLock(
      context(), url(), lock_type,
      base::BindOnce(&FileSystemAccessFileHandleImpl::StartCreateSwapFile,
                     weak_factory_.GetWeakPtr(), 0, keep_existing_data,
                     auto_close, std::move(callback)));
}

void FileSystemAccessFileHandleImpl::StartCreateSwapFile(
    int start_count,
    bool keep_existing_data,
    bool auto_close,
    CreateFileWriterCallback callback,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(start_count >= 0);
  DCHECK(max_swap_files_ >= 0);

  // We should not have gotten any farther than zero without a lock on the file.
  CHECK(start_count == 0 || lock);

  if (!lock) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kNoModificationAllowedError),
        mojo::NullRemote());
    return;
  }

  if (GetWritePermissionStatus() != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kPermissionDenied),
                            mojo::NullRemote());
    return;
  }

  for (int count = start_count; count < max_swap_files_; count++) {
    auto swap_name =
        url().virtual_path().BaseName().AddExtensionASCII(".crswap");

    if (count > 0) {
      swap_name = swap_name.InsertBeforeExtensionASCII(
          base::StringPrintf(".%d", count));
    }

    // First attempt to just create the swap file in the same directory (and
    // file system) as this file.
    std::optional<base::SafeBaseName> opt_swap_name =
        base::SafeBaseName::Create(swap_name);
    CHECK(opt_swap_name.has_value());
#if BUILDFLAG(IS_ANDROID)
    //  For content-URIs (e.g. content://com.android.../doc/msf%3A123), we will
    //  write the swap file to the local cache dir
    //  (e.g. /data/user/0/com.chrome.dev/cache/FileSystemAPISwap) and then
    //  copy back to the original content-URI when done.
    storage::FileSystemURL swap_url;
    if (url().path().IsContentUri()) {
      // We must escape 'content://com.android...' to use it as the file name.
      std::string file_name = base::EscapeAllExceptUnreserved(
          url().path().DirName().Append(*opt_swap_name).value());
      swap_url = manager()->CreateFileSystemURLFromPath(
          PathInfo(swap_dir_.Append(file_name)));
    } else {
      swap_url = url().CreateSibling(*opt_swap_name);
    }
#else
    storage::FileSystemURL swap_url = url().CreateSibling(*opt_swap_name);
#endif
    CHECK(swap_url.is_valid());

    // Check if this swap file is not in use. If it isn't, take a lock on it.
    if (!manager()->IsContentious(swap_url,
                                  manager()->GetExclusiveLockType())) {
      manager()->TakeLock(
          context(), swap_url, manager()->GetExclusiveLockType(),
          base::BindOnce(&FileSystemAccessFileHandleImpl::DidTakeSwapLock,
                         weak_factory_.GetWeakPtr(), count, swap_url,
                         keep_existing_data, auto_close, std::move(lock),
                         std::move(callback)));
      return;
    }
  }

  DLOG(ERROR) << "Error Creating Swap File, exceeded max unique files of: "
              << max_swap_files_
              << " base name: " << url().virtual_path().BaseName();
  std::move(callback).Run(file_system_access_error::FromStatus(
                              FileSystemAccessStatus::kOperationFailed,
                              "Failed to create swap file."),
                          mojo::NullRemote());
}

void FileSystemAccessFileHandleImpl::DidTakeSwapLock(
    int count,
    const storage::FileSystemURL& swap_url,
    bool keep_existing_data,
    bool auto_close,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    CreateFileWriterCallback callback,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(lock);

  // Taking `swap_lock` should succeed since we checked for contention ahead of
  // time.
  CHECK(swap_lock);

  if (keep_existing_data) {
    // Check whether a file exists at the intended path of the swap file.

    // TOCTOU errors are possible if a file at the path is created between the
    // existence check and when file contents are copied to the new file.
    // However, since we've acquired an exclusive lock to the swap file, this
    // is only possible if the file is created external to this API.
    // TODO(crbug.com/40245515): Consider requiring a lock to create an
    // empty file, e.g. parent.getFileHandle(swapFileName, {create: true}).
    manager()->DoFileSystemOperation(
        FROM_HERE, &FileSystemOperationRunner::FileExists,
        base::BindOnce(&FileSystemAccessFileHandleImpl::DidCheckSwapFileExists,
                       weak_factory_.GetWeakPtr(), count, swap_url, auto_close,
                       std::move(lock), std::move(swap_lock),
                       std::move(callback)),
        swap_url);
  } else {
    // Create an empty file. Passing the `exclusive` flag means this will fail
    // if a file already exists.
    manager()->DoFileSystemOperation(
        FROM_HERE, &FileSystemOperationRunner::CreateFile,
        base::BindOnce(&FileSystemAccessFileHandleImpl::DidCreateSwapFile,
                       weak_factory_.GetWeakPtr(), count, swap_url,
                       /*keep_existing_data=*/false, auto_close,
                       std::move(lock), std::move(swap_lock),
                       std::move(callback)),
        swap_url,
        /*exclusive=*/true);
  }
}

void FileSystemAccessFileHandleImpl::DidCheckSwapFileExists(
    int count,
    const storage::FileSystemURL& swap_url,
    bool auto_close,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
    CreateFileWriterCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(count >= 0);
  DCHECK(max_swap_files_ >= 0);

  if (result != base::File::FILE_ERROR_NOT_FOUND) {
    // File already exists. We need to find an unused filename.
    StartCreateSwapFile(count + 1, /*keep_existing_data=*/true, auto_close,
                        std::move(callback), std::move(lock));
    return;
  }

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40255657): Expand use of copy-on-write swap files
  // to other file systems which support it.
  if (CanUseCowSwapFile()) {
    CreateClonedSwapFile(count, swap_url, auto_close, std::move(lock),
                         std::move(swap_lock), std::move(callback));
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  CreateSwapFileFromCopy(count, swap_url, auto_close, std::move(lock),
                         std::move(swap_lock), std::move(callback));
}

void FileSystemAccessFileHandleImpl::CreateSwapFileFromCopy(
    int count,
    const storage::FileSystemURL& swap_url,
    bool auto_close,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
    CreateFileWriterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(count >= 0);
  DCHECK(max_swap_files_ >= 0);

  manager()->DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::Copy,
      base::BindOnce(&FileSystemAccessFileHandleImpl::DidCreateSwapFile,
                     weak_factory_.GetWeakPtr(), count, swap_url,
                     /*keep_existing_data=*/true, auto_close, std::move(lock),
                     std::move(swap_lock), std::move(callback)),
      url(), swap_url,
      storage::FileSystemOperation::CopyOrMoveOptionSet(
          {storage::FileSystemOperation::CopyOrMoveOption::
               kPreserveLastModified}),
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      std::make_unique<storage::CopyOrMoveHookDelegate>());
}

#if BUILDFLAG(IS_MAC)
void FileSystemAccessFileHandleImpl::CreateClonedSwapFile(
    int count,
    const storage::FileSystemURL& swap_url,
    bool auto_close,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
    CreateFileWriterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(count >= 0);
  DCHECK(max_swap_files_ >= 0);
  DCHECK(CanUseCowSwapFile());

  auto after_clone_callback = base::BindOnce(
      &FileSystemAccessFileHandleImpl::DidCloneSwapFile,
      weak_factory_.GetWeakPtr(), count, swap_url, auto_close, std::move(lock),
      std::move(swap_lock), std::move(callback));

  if (swap_file_cloning_will_fail_for_testing_) {
    std::move(after_clone_callback).Run(base::File::Error::FILE_ERROR_ABORT);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CreateCowSwapFile, url(), swap_url),
      std::move(after_clone_callback));
}

void FileSystemAccessFileHandleImpl::DidCloneSwapFile(
    int count,
    const storage::FileSystemURL& swap_url,
    bool auto_close,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
    CreateFileWriterCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanUseCowSwapFile());

  swap_file_clone_result_for_testing_ = result;

  if (result == base::File::FILE_ERROR_EXISTS) {
    // Cloning fails if the destination file exists. The file must have been
    // created between the FileExists check and the clone attempt. Attempt to
    // find another unused filename.
    StartCreateSwapFile(count + 1, /*keep_existing_data=*/true, auto_close,
                        std::move(callback), std::move(lock));
    return;
  }

  if (result != base::File::FILE_OK) {
    // Cloning could fail if the file's underlying file system does not support
    // copy-on-write, such as when accessing FAT formatted external USB drives
    // (which do not support copy-on-write) from a Mac (which otherwise does).
    // In that case, fall back on the create + copy technique.
    CreateSwapFileFromCopy(count, swap_url, auto_close, std::move(lock),
                           std::move(swap_lock), std::move(callback));
    return;
  }

  std::move(callback).Run(
      file_system_access_error::Ok(),
      manager()->CreateFileWriter(
          context(), url(), swap_url, std::move(lock), std::move(swap_lock),
          FileSystemAccessManagerImpl::SharedHandleState(
              handle_state().read_grant, handle_state().write_grant),
          auto_close));
}
#endif  // BUILDFLAG(IS_MAC)

void FileSystemAccessFileHandleImpl::DidCreateSwapFile(
    int count,
    const storage::FileSystemURL& swap_url,
    bool keep_existing_data,
    bool auto_close,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
    CreateFileWriterCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == base::File::FILE_ERROR_EXISTS) {
    // Creation attempt failed. We need to find an unused filename.
    StartCreateSwapFile(count + 1, keep_existing_data, auto_close,
                        std::move(callback), std::move(lock));
    return;
  }

  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Error Creating Swap File, status: "
                << base::File::ErrorToString(result)
                << " path: " << swap_url.path();
    std::move(callback).Run(file_system_access_error::FromFileError(
                                result, "Error creating swap file."),
                            mojo::NullRemote());
    return;
  }

  std::move(callback).Run(
      file_system_access_error::Ok(),
      manager()->CreateFileWriter(
          context(), url(), swap_url, std::move(lock), std::move(swap_lock),
          FileSystemAccessManagerImpl::SharedHandleState(
              handle_state().read_grant, handle_state().write_grant),
          auto_close));
}

void FileSystemAccessFileHandleImpl::GetUniqueId(GetUniqueIdCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto id = manager()->GetUniqueId(*this);
  DCHECK(id.is_valid());
  std::move(callback).Run(file_system_access_error::Ok(),
                          id.AsLowercaseString());
}

#if BUILDFLAG(IS_MAC)
bool FileSystemAccessFileHandleImpl::CanUseCowSwapFile() const {
  return url().type() == storage::kFileSystemTypeLocal;
}
#endif  // BUILDFLAG(IS_MAC)

void FileSystemAccessFileHandleImpl::GetCloudIdentifiers(
    GetCloudIdentifiersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoGetCloudIdentifiers(FileSystemAccessPermissionContext::HandleType::kFile,
                        std::move(callback));
}

base::WeakPtr<FileSystemAccessHandleBase>
FileSystemAccessFileHandleImpl::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
