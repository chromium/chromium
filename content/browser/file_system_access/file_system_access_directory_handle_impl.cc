// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"

#include <optional>

#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/file_util_icu.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "net/base/mime_util.h"
#endif

using blink::mojom::FileSystemAccessEntry;
using blink::mojom::FileSystemAccessEntryPtr;
using blink::mojom::FileSystemAccessHandle;
using blink::mojom::FileSystemAccessStatus;
using blink::mojom::FileSystemAccessTransferToken;
using storage::FileSystemOperationRunner;

namespace content {

using HandleType = FileSystemAccessPermissionContext::HandleType;
using SensitiveEntryResult =
    FileSystemAccessPermissionContext::SensitiveEntryResult;
using UserAction = FileSystemAccessPermissionContext::UserAction;

struct FileSystemAccessDirectoryHandleImpl::
    FileSystemAccessDirectoryEntriesListenerHolder
    : base::RefCountedDeleteOnSequence<
          FileSystemAccessDirectoryEntriesListenerHolder> {
  FileSystemAccessDirectoryEntriesListenerHolder(
      mojo::PendingRemote<
          blink::mojom::FileSystemAccessDirectoryEntriesListener>
          pending_listener,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : base::RefCountedDeleteOnSequence<
            FileSystemAccessDirectoryEntriesListenerHolder>(
            std::move(task_runner)),
        listener(mojo::Remote<
                 blink::mojom::FileSystemAccessDirectoryEntriesListener>(
            std::move(pending_listener))) {
    listener.reset_on_disconnect();
  }

  FileSystemAccessDirectoryEntriesListenerHolder(
      const FileSystemAccessDirectoryEntriesListenerHolder&) = delete;
  FileSystemAccessDirectoryEntriesListenerHolder& operator=(
      const FileSystemAccessDirectoryEntriesListenerHolder&) = delete;

  mojo::Remote<blink::mojom::FileSystemAccessDirectoryEntriesListener> listener;

  // Tracks the number of invocation of
  // FileSystemAccessDirectoryHandleImpl::DidReadDirectory.
  int32_t total_batch_count{0};

  // The termination of each call of
  // FileSystemAccessDirectoryHandleImpl::DidReadDirectory will trigger a call
  // to FileSystemAccessDirectoryHandleImpl::CurrentBatchEntriesReady. This
  // counter tracks the number of calls to
  // FileSystemAccessDirectoryHandleImpl::CurrentBatchEntriesReady.
  int32_t processed_batch_count{0};

  // Tracks whether the final entries have been received. This is used to
  // determine whether the listener should expect more entries.
  bool has_received_final_batch{false};

 private:
  ~FileSystemAccessDirectoryEntriesListenerHolder() = default;
  friend class base::RefCountedDeleteOnSequence<
      FileSystemAccessDirectoryEntriesListenerHolder>;
  friend class base::DeleteHelper<
      FileSystemAccessDirectoryEntriesListenerHolder>;
};

FileSystemAccessDirectoryHandleImpl::FileSystemAccessDirectoryHandleImpl(
    FileSystemAccessManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : FileSystemAccessHandleBase(manager, context, url, handle_state) {}

FileSystemAccessDirectoryHandleImpl::~FileSystemAccessDirectoryHandleImpl() =
    default;

void FileSystemAccessDirectoryHandleImpl::GetPermissionStatus(
    bool writable,
    GetPermissionStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoGetPermissionStatus(writable, std::move(callback));
}

void FileSystemAccessDirectoryHandleImpl::RequestPermission(
    bool writable,
    RequestPermissionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoRequestPermission(writable, std::move(callback));
}

void FileSystemAccessDirectoryHandleImpl::GetFile(const std::string& basename,
                                                  bool create,
                                                  GetFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  // Lookup content-URI by display-name. If the file does not exist, and
  // `create` is set, ContentUriGetChildDocumentOrQuery() will return a
  // create-child-document query URI which is used in
  // NativeFileUtil::EnsureFileExists() to call ContentUriGetDocumentFromQuery()
  // and create the document. DidGetFile() will then update the child path
  // before creating the returned handle.
  if (url().virtual_path().IsContentUri()) {
    std::string mime_type;
    std::string ext = base::FilePath(basename).Extension();
    if (ext.empty() ||
        !net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type)) {
      mime_type = "application/octet-stream";
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&base::ContentUriGetChildDocumentOrQuery,
                       url().virtual_path(), basename, mime_type,
                       /*is_directory=*/false, create),
        base::BindOnce(
            &FileSystemAccessDirectoryHandleImpl::OnGetFileContentUri,
            weak_factory_.GetWeakPtr(), basename, create, std::move(callback)));
    return;
  }
#endif

  storage::FileSystemURL child_url;
  blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  GetFileResolved(basename, create, std::move(callback),
                  std::move(get_child_url_result), std::move(child_url));
}

#if BUILDFLAG(IS_ANDROID)
void FileSystemAccessDirectoryHandleImpl::OnGetFileContentUri(
    std::string basename,
    bool create,
    GetFileCallback callback,
    base::FilePath child_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (child_path.empty()) {
    std::move(callback).Run(file_system_access_error::FromFileError(
                                base::File::FILE_ERROR_NOT_FOUND),
                            mojo::NullRemote());
    return;
  }

  GetFileResolved(basename, create, std::move(callback),
                  file_system_access_error::Ok(), CreateChildURL(child_path));
}
#endif

void FileSystemAccessDirectoryHandleImpl::GetFileResolved(
    const std::string& basename,
    bool create,
    GetFileCallback callback,
    blink::mojom::FileSystemAccessErrorPtr get_child_url_result,
    storage::FileSystemURL child_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (get_child_url_result->status != FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result),
                            mojo::NullRemote());
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kPermissionDenied),
                            mojo::NullRemote());
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessDirectoryIterationBlocklistCheck) &&
      manager()->permission_context()) {
    // While this directory handle already has obtained the permission and
    // checked for the blocklist, a child symlink file may have been created
    // since then, pointing to a blocklisted file or directory.  Check for
    // sensitive entry access, which is run on the resolved path.
    PathInfo path_info{
        child_url.type() == storage::FileSystemType::kFileSystemTypeLocal
            ? PathType::kLocal
            : PathType::kExternal,
        child_url.path(), basename};
    manager()->permission_context()->ConfirmSensitiveEntryAccess(
        context().storage_key.origin(), path_info, HandleType::kFile,
        UserAction::kNone, context().frame_id,
        base::BindOnce(&FileSystemAccessDirectoryHandleImpl::DoGetFile,
                       weak_factory_.GetWeakPtr(), create, child_url,
                       std::move(callback)));
    return;
  }

  DoGetFile(create, child_url, std::move(callback),
            SensitiveEntryResult::kAllowed);
}

void FileSystemAccessDirectoryHandleImpl::DoGetFile(
    bool create,
    storage::FileSystemURL url,
    GetFileCallback callback,
    SensitiveEntryResult sensitive_entry_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sensitive_entry_result != SensitiveEntryResult::kAllowed) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kSecurityError),
                            mojo::NullRemote());
    return;
  }

  if (create) {
    // If `create` is true, write permission is required unconditionally, i.e.
    // even if the file already exists. This is intentional, and matches the
    // behavior that is specified in the spec.
    RunWithWritePermission(
        base::BindOnce(
            &FileSystemAccessDirectoryHandleImpl::GetFileWithWritePermission,
            weak_factory_.GetWeakPtr(), url),
        base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                          GetFileCallback callback) {
          std::move(callback).Run(std::move(result), mojo::NullRemote());
        }),
        std::move(callback));
  } else {
    manager()->DoFileSystemOperation(
        FROM_HERE, &FileSystemOperationRunner::FileExists,
        base::BindOnce(&FileSystemAccessDirectoryHandleImpl::DidGetFile,
                       weak_factory_.GetWeakPtr(), url, std::move(callback)),
        url);
  }
}

void FileSystemAccessDirectoryHandleImpl::GetDirectory(
    const std::string& basename,
    bool create,
    GetDirectoryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  // Lookup content-URI by display-name. If the directory does not exist, and
  // `create` is set, ContentUriGetChildDocumentOrQuery() will return a
  // create-child-document query URI which is used in
  // NativeFileUtil::CreateDirectory() to call ContentUriGetDocumentFromQuery()
  // and create the document. DidGetDirectory() will then update the child path
  // before creating the returned handle.
  if (url().virtual_path().IsContentUri()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&base::ContentUriGetChildDocumentOrQuery,
                       url().virtual_path(), basename,
                       /*mime_type=*/std::string(),
                       /*is_directory=*/true, create),
        base::BindOnce(
            &FileSystemAccessDirectoryHandleImpl::OnGetDirectoryContentUri,
            weak_factory_.GetWeakPtr(), basename, create, std::move(callback)));
    return;
  }
#endif

  storage::FileSystemURL child_url;
  blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  GetDirectoryResolved(basename, create, std::move(callback),
                       std::move(get_child_url_result), std::move(child_url));
}

#if BUILDFLAG(IS_ANDROID)
void FileSystemAccessDirectoryHandleImpl::OnGetDirectoryContentUri(
    std::string basename,
    bool create,
    GetDirectoryCallback callback,
    base::FilePath child_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (child_path.empty()) {
    std::move(callback).Run(file_system_access_error::FromFileError(
                                base::File::FILE_ERROR_NOT_FOUND),
                            mojo::NullRemote());
    return;
  }

  GetDirectoryResolved(basename, create, std::move(callback),
                       file_system_access_error::Ok(),
                       CreateChildURL(child_path));
}
#endif

void FileSystemAccessDirectoryHandleImpl::GetDirectoryResolved(
    const std::string& basename,
    bool create,
    GetDirectoryCallback callback,
    blink::mojom::FileSystemAccessErrorPtr get_child_url_result,
    storage::FileSystemURL child_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (get_child_url_result->status != FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result),
                            mojo::NullRemote());
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kPermissionDenied),
                            mojo::NullRemote());
    return;
  }

  if (create) {
    // If `create` is true, write permission is required unconditionally, i.e.
    // even if the file already exists. This is intentional, and matches the
    // behavior that is specified in the spec.
    RunWithWritePermission(
        base::BindOnce(&FileSystemAccessDirectoryHandleImpl::
                           GetDirectoryWithWritePermission,
                       weak_factory_.GetWeakPtr(), child_url),
        base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                          GetDirectoryCallback callback) {
          std::move(callback).Run(std::move(result), mojo::NullRemote());
        }),
        std::move(callback));
  } else {
    manager()->DoFileSystemOperation(
        FROM_HERE, &FileSystemOperationRunner::DirectoryExists,
        base::BindOnce(&FileSystemAccessDirectoryHandleImpl::DidGetDirectory,
                       weak_factory_.GetWeakPtr(), child_url,
                       std::move(callback)),
        child_url);
  }
}

void FileSystemAccessDirectoryHandleImpl::GetEntries(
    mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto listener_holder =
      base::MakeRefCounted<FileSystemAccessDirectoryEntriesListenerHolder>(
          std::move(pending_listener),
          base::SequencedTaskRunner::GetCurrentDefault());

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    listener_holder->listener->DidReadDirectory(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kPermissionDenied),
        {}, false);
    return;
  }

  manager()->DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::ReadDirectory,
      base::BindRepeating(
          &FileSystemAccessDirectoryHandleImpl::DidReadDirectory,
          weak_factory_.GetWeakPtr(), std::move(listener_holder)),
      url());
}

void FileSystemAccessDirectoryHandleImpl::Move(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        destination_directory,
    const std::string& new_entry_name,
    MoveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40198034): Implement move for directory handles.
  std::move(callback).Run(file_system_access_error::FromStatus(
      blink::mojom::FileSystemAccessStatus::kOperationAborted));
}

void FileSystemAccessDirectoryHandleImpl::Rename(
    const std::string& new_entry_name,
    RenameCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40198034): Implement rename for directory handles.
  std::move(callback).Run(file_system_access_error::FromStatus(
      blink::mojom::FileSystemAccessStatus::kOperationAborted));
}

void FileSystemAccessDirectoryHandleImpl::Remove(bool recurse,
                                                 RemoveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessHandleBase::DoRemove,
                     weak_factory_.GetWeakPtr(), url(), recurse),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        RemoveEntryCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

void FileSystemAccessDirectoryHandleImpl::RemoveEntry(
    const std::string& basename,
    bool recurse,
    RemoveEntryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  // Lookup content-URI by display-name.
  if (url().virtual_path().IsContentUri()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&base::ContentUriGetChildDocumentOrQuery,
                       url().virtual_path(), basename,
                       /*mime_type=*/std::string(),
                       /*is_directory=*/false, /*create=*/false),
        base::BindOnce(
            &FileSystemAccessDirectoryHandleImpl::OnRemoveEntryContentUri,
            weak_factory_.GetWeakPtr(), basename, recurse,
            std::move(callback)));
    return;
  }
#endif

  storage::FileSystemURL child_url;
  blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  RemoveEntryResolved(basename, recurse, std::move(callback),
                      std::move(get_child_url_result), std::move(child_url));
}

#if BUILDFLAG(IS_ANDROID)
void FileSystemAccessDirectoryHandleImpl::OnRemoveEntryContentUri(
    std::string basename,
    bool recurse,
    RemoveEntryCallback callback,
    base::FilePath child_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (child_path.empty()) {
    std::move(callback).Run(file_system_access_error::FromFileError(
        base::File::FILE_ERROR_NOT_FOUND));
    return;
  }

  RemoveEntryResolved(basename, recurse, std::move(callback),
                      file_system_access_error::Ok(),
                      CreateChildURL(child_path));
}
#endif

void FileSystemAccessDirectoryHandleImpl::RemoveEntryResolved(
    const std::string& basename,
    bool recurse,
    RemoveEntryCallback callback,
    blink::mojom::FileSystemAccessErrorPtr get_child_url_result,
    storage::FileSystemURL child_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (get_child_url_result->status != FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result));
    return;
  }

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessHandleBase::DoRemove,
                     weak_factory_.GetWeakPtr(), child_url, recurse),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        RemoveEntryCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

void FileSystemAccessDirectoryHandleImpl::Resolve(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        possible_child,
    ResolveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->ResolveTransferToken(
      std::move(possible_child),
      base::BindOnce(&FileSystemAccessDirectoryHandleImpl::ResolveImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemAccessDirectoryHandleImpl::ResolveImpl(
    ResolveCallback callback,
    FileSystemAccessTransferTokenImpl* possible_child) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!possible_child) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed),
        std::nullopt);
    return;
  }

  const storage::FileSystemURL& parent_url = url();
  const storage::FileSystemURL& child_url = possible_child->url();

  // If two URLs are of a different type they are definitely not related.
  if (parent_url.type() != child_url.type()) {
    std::move(callback).Run(file_system_access_error::Ok(), std::nullopt);
    return;
  }

  // URLs from the sandboxed file system must include bucket info, while URLs
  // from non-sandboxed file systems should not.
  DCHECK_EQ(parent_url.type() == storage::kFileSystemTypeTemporary,
            parent_url.bucket().has_value());
  DCHECK_EQ(child_url.type() == storage::kFileSystemTypeTemporary,
            child_url.bucket().has_value());

  // Since the types match, either both or neither URL will have bucket info.
  if (parent_url.bucket() != child_url.bucket()) {
    std::move(callback).Run(file_system_access_error::Ok(), std::nullopt);
    return;
  }

  // Otherwise compare path.
  const base::FilePath& parent_path = parent_url.path();
  const base::FilePath& child_path = child_url.path();

  // Same path, so return empty array if child is also a directory.
  if (parent_path == child_path) {
    std::move(callback).Run(file_system_access_error::Ok(),
                            possible_child->type() == HandleType::kDirectory
                                ? std::make_optional(std::vector<std::string>())
                                : std::nullopt);
    return;
  }

  // Now figure out relative path, if any.
  base::FilePath relative_path;
  if (parent_path.empty()) {
    // The root of a sandboxed file system will have an empty path. In that
    // case the child path is already the relative path.
    relative_path = child_path;
  } else if (!parent_path.AppendRelativePath(child_path, &relative_path)) {
    std::move(callback).Run(file_system_access_error::Ok(), std::nullopt);
    return;
  }

  std::vector<base::FilePath::StringType> components =
      relative_path.GetComponents();
#if BUILDFLAG(IS_WIN)
  std::vector<std::string> result;
  result.reserve(components.size());
  for (const auto& component : components) {
    result.push_back(base::WideToUTF8(component));
  }
  std::move(callback).Run(file_system_access_error::Ok(), std::move(result));
#else
  std::move(callback).Run(file_system_access_error::Ok(),
                          std::move(components));
#endif
}

void FileSystemAccessDirectoryHandleImpl::Transfer(
    mojo::PendingReceiver<FileSystemAccessTransferToken> token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->CreateTransferToken(*this, std::move(token));
}

void FileSystemAccessDirectoryHandleImpl::GetFileWithWritePermission(
    const storage::FileSystemURL& child_url,
    GetFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  manager()->DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::CreateFile,
      base::BindOnce(&FileSystemAccessDirectoryHandleImpl::DidGetFile,
                     weak_factory_.GetWeakPtr(), child_url,
                     std::move(callback)),
      child_url,
      /*exclusive=*/false);
}

#if BUILDFLAG(IS_ANDROID)
void FileSystemAccessDirectoryHandleImpl::DidGetFileQueryUri(
    GetFileCallback callback,
    base::FilePath child_path) {
  if (child_path.empty()) {
    DidGetFile(storage::FileSystemURL(), std::move(callback),
               base::File::FILE_ERROR_NOT_FOUND);
  } else {
    DidGetFile(CreateChildURL(child_path), std::move(callback),
               base::File::FILE_OK);
  }
}
#endif

void FileSystemAccessDirectoryHandleImpl::DidGetFile(
    storage::FileSystemURL child_url,
    GetFileCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  // If getFile() was called with `create` for a non-existing file, then
  // `child_url` will be a create-child-document URI which would have been used
  // in NativeFileUtils::EnsureFileExists() to create the document, and we need
  // to lookup the document URI.
  base::FilePath child_path = child_url.path();
  if (result == base::File::FILE_OK &&
      base::ContentUriIsCreateChildDocumentQuery(child_path)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&base::ContentUriGetDocumentFromQuery, child_path,
                       /*create=*/false),
        base::BindOnce(&FileSystemAccessDirectoryHandleImpl::DidGetFileQueryUri,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
#endif

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(file_system_access_error::FromFileError(result),
                            mojo::NullRemote());
    return;
  }

  std::move(callback).Run(
      file_system_access_error::Ok(),
      manager()->CreateFileHandle(context(), child_url, handle_state()));
}

void FileSystemAccessDirectoryHandleImpl::GetDirectoryWithWritePermission(
    const storage::FileSystemURL& child_url,
    GetDirectoryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  manager()->DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::CreateDirectory,
      base::BindOnce(&FileSystemAccessDirectoryHandleImpl::DidGetDirectory,
                     weak_factory_.GetWeakPtr(), child_url,
                     std::move(callback)),
      child_url,
      /*exclusive=*/false, /*recursive=*/false);
}

#if BUILDFLAG(IS_ANDROID)
void FileSystemAccessDirectoryHandleImpl::DidGetDirectoryQueryUri(
    GetDirectoryCallback callback,
    base::FilePath child_path) {
  if (child_path.empty()) {
    DidGetDirectory(storage::FileSystemURL(), std::move(callback),
                    base::File::FILE_ERROR_NOT_FOUND);
  } else {
    DidGetDirectory(CreateChildURL(child_path), std::move(callback),
                    base::File::FILE_OK);
  }
}
#endif

void FileSystemAccessDirectoryHandleImpl::DidGetDirectory(
    storage::FileSystemURL child_url,
    GetDirectoryCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  // If getDirectory() was called with `create` for a non-existing file, then
  // `child_url` will be a create-child-document URI which would have been used
  // in NativeFileUtils::CreateDirectory() to create the dir, and we need
  // to lookup the document URI.
  base::FilePath child_path = child_url.path();
  if (result == base::File::FILE_OK &&
      base::ContentUriIsCreateChildDocumentQuery(child_path)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&base::ContentUriGetDocumentFromQuery, child_path,
                       /*create=*/false),
        base::BindOnce(
            &FileSystemAccessDirectoryHandleImpl::DidGetDirectoryQueryUri,
            weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
#endif

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(file_system_access_error::FromFileError(result),
                            mojo::NullRemote());
    return;
  }

  std::move(callback).Run(
      file_system_access_error::Ok(),
      manager()->CreateDirectoryHandle(context(), child_url, handle_state()));
}

void FileSystemAccessDirectoryHandleImpl::DidReadDirectory(
    scoped_refptr<FileSystemAccessDirectoryEntriesListenerHolder>
        listener_holder,
    base::File::Error result,
    std::vector<filesystem::mojom::DirectoryEntry> file_list,
    bool has_more_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!listener_holder->listener) {
    return;
  }

  if (result != base::File::FILE_OK) {
    DCHECK(!has_more_entries);
    listener_holder->listener->DidReadDirectory(
        file_system_access_error::FromFileError(result), {}, false);
    return;
  }

  ++listener_holder->total_batch_count;
  listener_holder->has_received_final_batch = !has_more_entries;

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessDirectoryIterationBlocklistCheck) &&
      manager()->permission_context()) {
    // While this directory handle already has obtained the permission and
    // checked for the blocklist, a child symlink file may have been created
    // since then, pointing to a blocklisted file or directory. Before merging
    // a child into a result vector, check for sensitive entry access, which is
    // run on the resolved path.
    auto final_callback = base::BindOnce(
        &FileSystemAccessDirectoryHandleImpl::CurrentBatchEntriesReady,
        weak_factory_.GetWeakPtr(), std::move(listener_holder));

    // Barrier callback is used to wait for checking each path in the
    // `file_list` and creating a `FileSystemAccessEntryPtr` if the path is
    // valid; otherwise, nullptr is returned for the callback. Since the barrier
    // callback expects a fixed number of callbacks to be invoked before the
    // final callback is invoked, each item in `file_list` must trigger the
    // barrier callback with a valid `FileSystemAccessEntryPtr` or nullptr.
    auto barrier_callback = base::BarrierCallback<FileSystemAccessEntryPtr>(
        file_list.size(),
        base::BindOnce(
            &FileSystemAccessDirectoryHandleImpl::MergeCurrentBatchEntries,
            weak_factory_.GetWeakPtr(), std::move(final_callback)));

    for (const auto& entry : file_list) {
      std::string basename = storage::FilePathToString(entry.name.path());
      storage::FileSystemURL child_url;
      blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
          GetChildURL(basename, &child_url);

      // Skip any entries with names that aren't allowed to be accessed by
      // this API, such as files with disallowed characters in their names.
      if (get_child_url_result->status != FileSystemAccessStatus::kOk) {
        barrier_callback.Run(nullptr);
        continue;
      }

      if (entry.type == filesystem::mojom::FsFileType::DIRECTORY) {
        auto directory_result_entry = CreateEntry(
            entry.name, entry.display_name, child_url, HandleType::kDirectory);
        barrier_callback.Run(std::move(directory_result_entry));
        continue;
      }

      // Only run sensitive entry check on a file, which could be a symbolic
      // link.
      manager()->permission_context()->ConfirmSensitiveEntryAccess(
          context().storage_key.origin(),
          PathInfo(
              child_url.type() == storage::FileSystemType::kFileSystemTypeLocal
                  ? PathType::kLocal
                  : PathType::kExternal,
              child_url.path(), entry.name.AsUTF8Unsafe()),
          HandleType::kFile, UserAction::kNone, context().frame_id,
          base::BindOnce(&FileSystemAccessDirectoryHandleImpl::
                             DidVerifySensitiveAccessForFileEntry,
                         weak_factory_.GetWeakPtr(), entry.name,
                         entry.display_name, child_url, barrier_callback));
    }
    return;
  }

  std::vector<FileSystemAccessEntryPtr> entries;
  for (const auto& entry : file_list) {
    std::string basename = storage::FilePathToString(entry.name.path());

    storage::FileSystemURL child_url;
    blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
        GetChildURL(basename, &child_url);

    // Skip any entries with names that aren't allowed to be accessed by
    // this API, such as files with disallowed characters in their names.
    if (get_child_url_result->status != FileSystemAccessStatus::kOk) {
      continue;
    }

    entries.push_back(
        CreateEntry(entry.name, entry.display_name, child_url,
                    entry.type == filesystem::mojom::FsFileType::DIRECTORY
                        ? HandleType::kDirectory
                        : HandleType::kFile));
  }
  CurrentBatchEntriesReady(std::move(listener_holder), std::move(entries));
}

void FileSystemAccessDirectoryHandleImpl::DidVerifySensitiveAccessForFileEntry(
    base::SafeBaseName basename,
    std::string display_name,
    storage::FileSystemURL child_url,
    base::OnceCallback<void(FileSystemAccessEntryPtr)> barrier_callback,
    FileSystemAccessPermissionContext::SensitiveEntryResult
        sensitive_entry_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sensitive_entry_result != SensitiveEntryResult::kAllowed) {
    std::move(barrier_callback).Run(nullptr);
    return;
  }

  auto entry =
      CreateEntry(basename, display_name, child_url, HandleType::kFile);
  std::move(barrier_callback).Run(std::move(entry));
}

void FileSystemAccessDirectoryHandleImpl::MergeCurrentBatchEntries(
    base::OnceCallback<void(std::vector<FileSystemAccessEntryPtr>)>
        final_callback,
    std::vector<FileSystemAccessEntryPtr> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<FileSystemAccessEntryPtr> filtered_entries;
  for (auto& entry : entries) {
    // Filter any entry that did not pass the access check.
    if (entry) {
      filtered_entries.push_back(std::move(entry));
    }
  }
  std::move(final_callback).Run(std::move(filtered_entries));
}

void FileSystemAccessDirectoryHandleImpl::CurrentBatchEntriesReady(
    scoped_refptr<FileSystemAccessDirectoryEntriesListenerHolder>
        listener_holder,
    std::vector<FileSystemAccessEntryPtr> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!listener_holder->listener) {
    return;
  }

  ++listener_holder->processed_batch_count;

  const bool all_batches_are_processed =
      listener_holder->processed_batch_count ==
      listener_holder->total_batch_count;
  const bool more_batches_are_expected =
      !all_batches_are_processed || !listener_holder->has_received_final_batch;

  listener_holder->listener->DidReadDirectory(file_system_access_error::Ok(),
                                              std::move(entries),
                                              more_batches_are_expected);
}

blink::mojom::FileSystemAccessErrorPtr
FileSystemAccessDirectoryHandleImpl::GetChildURL(
    const std::string& basename,
    storage::FileSystemURL* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const storage::FileSystemURL& parent = url();
  if (!manager()->IsSafePathComponent(
          parent.type(), context().storage_key.origin(), basename)) {
    return file_system_access_error::FromStatus(
        FileSystemAccessStatus::kInvalidArgument, "Name is not allowed.");
  }

#if BUILDFLAG(IS_ANDROID)
  base::FilePath child_path =
      parent.virtual_path().IsContentUri()
          ? ContentUriBuildDocumentUriUsingTree(parent.virtual_path(), basename)
          : parent.virtual_path().Append(basename);
  // If parent is not a Document Tree URI and basename not a document-id,
  // child_path will not be valid.
  if (child_path.empty()) {
    return file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidModificationError);
  }
#else
  base::FilePath child_path =
      parent.virtual_path().Append(base::FilePath::FromUTF8Unsafe(basename));
#endif
  *result = CreateChildURL(child_path);
  return file_system_access_error::Ok();
}

storage::FileSystemURL FileSystemAccessDirectoryHandleImpl::CreateChildURL(
    const base::FilePath& child_path) {
  const storage::FileSystemURL& parent = url();
  storage::FileSystemURL child_url =
      file_system_context()->CreateCrackedFileSystemURL(
          parent.storage_key(), parent.mount_type(), child_path);
  // Child URLs inherit their parent's storage bucket.
  if (parent.bucket()) {
    child_url.SetBucket(parent.bucket().value());
  }
  return child_url;
}

FileSystemAccessEntryPtr FileSystemAccessDirectoryHandleImpl::CreateEntry(
    const base::SafeBaseName& basename,
    const std::string& display_name,
    const storage::FileSystemURL& url,
    HandleType handle_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string name =
      !display_name.empty() ? display_name : basename.AsUTF8Unsafe();
  if (handle_type == HandleType::kDirectory) {
    return FileSystemAccessEntry::New(
        FileSystemAccessHandle::NewDirectory(
            manager()->CreateDirectoryHandle(context(), url, handle_state())),
        name);
  }
  return FileSystemAccessEntry::New(
      FileSystemAccessHandle::NewFile(
          manager()->CreateFileHandle(context(), url, handle_state())),
      name);
}

void FileSystemAccessDirectoryHandleImpl::GetUniqueId(
    GetUniqueIdCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Uuid id = manager()->GetUniqueId(*this);
  DCHECK(id.is_valid());
  std::move(callback).Run(file_system_access_error::Ok(),
                          id.AsLowercaseString());
}

void FileSystemAccessDirectoryHandleImpl::GetCloudIdentifiers(
    GetCloudIdentifiersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoGetCloudIdentifiers(
      FileSystemAccessPermissionContext::HandleType::kDirectory,
      std::move(callback));
}

base::WeakPtr<FileSystemAccessHandleBase>
FileSystemAccessDirectoryHandleImpl::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
