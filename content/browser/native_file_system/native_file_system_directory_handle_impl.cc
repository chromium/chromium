// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/native_file_system/native_file_system_error.h"
#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/escape.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_handle.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom.h"

using blink::mojom::NativeFileSystemEntry;
using blink::mojom::NativeFileSystemEntryPtr;
using blink::mojom::NativeFileSystemHandle;
using blink::mojom::NativeFileSystemStatus;
using blink::mojom::NativeFileSystemTransferToken;
using storage::FileSystemOperationRunner;

namespace content {

namespace {

// Returns true when |name| contains a path separator like "/".
bool ContainsPathSeparator(const std::string& name) {
  const base::FilePath filepath_name = storage::StringToFilePath(name);

  const size_t separator_position =
      filepath_name.value().find_first_of(base::FilePath::kSeparators);

  return separator_position != base::FilePath::StringType::npos;
}

// Returns true when |name| is "." or "..".
bool IsCurrentOrParentDirectory(const std::string& name) {
  const base::FilePath filepath_name = storage::StringToFilePath(name);
  return filepath_name.value() == base::FilePath::kCurrentDirectory ||
         filepath_name.value() == base::FilePath::kParentDirectory;
}

}  // namespace

NativeFileSystemDirectoryHandleImpl::NativeFileSystemDirectoryHandleImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : NativeFileSystemHandleBase(manager,
                                 context,
                                 url,
                                 handle_state,
                                 /*is_directory=*/true) {}

NativeFileSystemDirectoryHandleImpl::~NativeFileSystemDirectoryHandleImpl() =
    default;

void NativeFileSystemDirectoryHandleImpl::GetPermissionStatus(
    bool writable,
    GetPermissionStatusCallback callback) {
  DoGetPermissionStatus(writable, std::move(callback));
}

void NativeFileSystemDirectoryHandleImpl::RequestPermission(
    bool writable,
    RequestPermissionCallback callback) {
  DoRequestPermission(writable, std::move(callback));
}

void NativeFileSystemDirectoryHandleImpl::GetFile(const std::string& basename,
                                                  bool create,
                                                  GetFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage::FileSystemURL child_url;
  blink::mojom::NativeFileSystemErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  if (get_child_url_result->status != NativeFileSystemStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result),
                            mojo::NullRemote());
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kPermissionDenied),
                            mojo::NullRemote());
    return;
  }

  if (create) {
    // If |create| is true, write permission is required unconditionally, i.e.
    // even if the file already exists. This is intentional, and matches the
    // behavior that is specified in the spec.
    RunWithWritePermission(
        base::BindOnce(
            &NativeFileSystemDirectoryHandleImpl::GetFileWithWritePermission,
            weak_factory_.GetWeakPtr(), child_url),
        base::BindOnce([](GetFileCallback callback) {
          std::move(callback).Run(
              native_file_system_error::FromStatus(
                  NativeFileSystemStatus::kPermissionDenied),
              mojo::NullRemote());
        }),
        std::move(callback));
  } else {
    DoFileSystemOperation(
        FROM_HERE, &FileSystemOperationRunner::FileExists,
        base::BindOnce(&NativeFileSystemDirectoryHandleImpl::DidGetFile,
                       weak_factory_.GetWeakPtr(), child_url,
                       std::move(callback)),
        child_url);
  }
}

void NativeFileSystemDirectoryHandleImpl::GetDirectory(
    const std::string& basename,
    bool create,
    GetDirectoryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage::FileSystemURL child_url;
  blink::mojom::NativeFileSystemErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  if (get_child_url_result->status != NativeFileSystemStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result),
                            mojo::NullRemote());
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kPermissionDenied),
                            mojo::NullRemote());
    return;
  }

  if (create) {
    // If |create| is true, write permission is required unconditionally, i.e.
    // even if the file already exists. This is intentional, and matches the
    // behavior that is specified in the spec.
    RunWithWritePermission(
        base::BindOnce(&NativeFileSystemDirectoryHandleImpl::
                           GetDirectoryWithWritePermission,
                       weak_factory_.GetWeakPtr(), child_url),
        base::BindOnce([](GetDirectoryCallback callback) {
          std::move(callback).Run(
              native_file_system_error::FromStatus(
                  NativeFileSystemStatus::kPermissionDenied),
              mojo::NullRemote());
        }),
        std::move(callback));
  } else {
    DoFileSystemOperation(
        FROM_HERE, &FileSystemOperationRunner::DirectoryExists,
        base::BindOnce(&NativeFileSystemDirectoryHandleImpl::DidGetDirectory,
                       weak_factory_.GetWeakPtr(), child_url,
                       std::move(callback)),
        child_url);
  }
}

void NativeFileSystemDirectoryHandleImpl::GetEntries(
    mojo::PendingRemote<blink::mojom::NativeFileSystemDirectoryEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto listener = std::make_unique<
      mojo::Remote<blink::mojom::NativeFileSystemDirectoryEntriesListener>>(
      std::move(pending_listener));
  listener->reset_on_disconnect();

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::ReadDirectory,
      base::BindRepeating(
          &NativeFileSystemDirectoryHandleImpl::DidReadDirectory,
          weak_factory_.GetWeakPtr(), base::Owned(std::move(listener))),
      url());
}

void NativeFileSystemDirectoryHandleImpl::RemoveEntry(
    const std::string& basename,
    bool recurse,
    RemoveEntryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage::FileSystemURL child_url;
  blink::mojom::NativeFileSystemErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  if (get_child_url_result->status != NativeFileSystemStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result));
    return;
  }

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemDirectoryHandleImpl::RemoveEntryImpl,
                     weak_factory_.GetWeakPtr(), child_url, recurse),
      base::BindOnce([](RemoveEntryCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied));
      }),
      std::move(callback));
}

void NativeFileSystemDirectoryHandleImpl::Transfer(
    mojo::PendingReceiver<NativeFileSystemTransferToken> token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->CreateTransferToken(*this, std::move(token));
}

void NativeFileSystemDirectoryHandleImpl::GetFileWithWritePermission(
    const storage::FileSystemURL& child_url,
    GetFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::CreateFile,
      base::BindOnce(&NativeFileSystemDirectoryHandleImpl::DidGetFile,
                     weak_factory_.GetWeakPtr(), child_url,
                     std::move(callback)),
      child_url,
      /*exclusive=*/false);
}

void NativeFileSystemDirectoryHandleImpl::DidGetFile(
    const storage::FileSystemURL& url,
    GetFileCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(native_file_system_error::FromFileError(result),
                            mojo::NullRemote());
    return;
  }

  std::move(callback).Run(
      native_file_system_error::Ok(),
      manager()->CreateFileHandle(context(), url, handle_state()));
}

void NativeFileSystemDirectoryHandleImpl::GetDirectoryWithWritePermission(
    const storage::FileSystemURL& child_url,
    GetDirectoryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::CreateDirectory,
      base::BindOnce(&NativeFileSystemDirectoryHandleImpl::DidGetDirectory,
                     weak_factory_.GetWeakPtr(), child_url,
                     std::move(callback)),
      child_url,
      /*exclusive=*/false, /*recursive=*/false);
}

void NativeFileSystemDirectoryHandleImpl::DidGetDirectory(
    const storage::FileSystemURL& url,
    GetDirectoryCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(native_file_system_error::FromFileError(result),
                            mojo::NullRemote());
    return;
  }

  std::move(callback).Run(
      native_file_system_error::Ok(),
      manager()->CreateDirectoryHandle(context(), url, handle_state()));
}

void NativeFileSystemDirectoryHandleImpl::DidReadDirectory(
    mojo::Remote<blink::mojom::NativeFileSystemDirectoryEntriesListener>*
        listener,
    base::File::Error result,
    std::vector<filesystem::mojom::DirectoryEntry> file_list,
    bool has_more_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!*listener)
    return;

  if (result != base::File::FILE_OK) {
    DCHECK(!has_more_entries);
    (*listener)->DidReadDirectory(
        native_file_system_error::FromFileError(result), {}, false);
    return;
  }

  std::vector<NativeFileSystemEntryPtr> entries;
  for (const auto& entry : file_list) {
    std::string basename = storage::FilePathToString(entry.name);

    storage::FileSystemURL child_url;
    blink::mojom::NativeFileSystemErrorPtr get_child_url_result =
        GetChildURL(basename, &child_url);

    // All entries must exist in this directory as a direct child with a valid
    // |basename|.
    CHECK_EQ(get_child_url_result->status, NativeFileSystemStatus::kOk);

    entries.push_back(
        CreateEntry(basename, child_url,
                    entry.type == filesystem::mojom::FsFileType::DIRECTORY));
  }
  (*listener)->DidReadDirectory(native_file_system_error::Ok(),
                                std::move(entries), has_more_entries);
}

void NativeFileSystemDirectoryHandleImpl::RemoveEntryImpl(
    const storage::FileSystemURL& url,
    bool recurse,
    RemoveEntryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::Remove,
      base::BindOnce(
          [](RemoveEntryCallback callback, base::File::Error result) {
            std::move(callback).Run(
                native_file_system_error::FromFileError(result));
          },
          std::move(callback)),
      url, recurse);
}

blink::mojom::NativeFileSystemErrorPtr
NativeFileSystemDirectoryHandleImpl::GetChildURL(
    const std::string& basename,
    storage::FileSystemURL* result) {
  // TODO(mek): Rather than doing URL serialization and parsing we should just
  // have a way to get a child FileSystemURL directly from its parent.

  if (basename.empty()) {
    return native_file_system_error::FromStatus(
        NativeFileSystemStatus::kInvalidArgument,
        "Name can't be an empty string.");
  }

  if (ContainsPathSeparator(basename) || IsCurrentOrParentDirectory(basename)) {
    // |basename| must refer to a entry that exists in this directory as a
    // direct child.
    return native_file_system_error::FromStatus(
        NativeFileSystemStatus::kInvalidArgument,
        "Name contains invalid characters.");
  }

  std::string escaped_name =
      net::EscapeQueryParamValue(basename, /*use_plus=*/false);

  GURL parent_url = url().ToGURL();
  std::string path = base::StrCat({parent_url.path(), "/", escaped_name});
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  GURL child_url = parent_url.ReplaceComponents(replacements);

  *result = file_system_context()->CrackURL(child_url);
  return native_file_system_error::Ok();
}

NativeFileSystemEntryPtr NativeFileSystemDirectoryHandleImpl::CreateEntry(
    const std::string& basename,
    const storage::FileSystemURL& url,
    bool is_directory) {
  if (is_directory) {
    return NativeFileSystemEntry::New(
        NativeFileSystemHandle::NewDirectory(
            manager()->CreateDirectoryHandle(context(), url, handle_state())),
        basename);
  }
  return NativeFileSystemEntry::New(
      NativeFileSystemHandle::NewFile(
          manager()->CreateFileHandle(context(), url, handle_state())),
      basename);
}

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemDirectoryHandleImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
