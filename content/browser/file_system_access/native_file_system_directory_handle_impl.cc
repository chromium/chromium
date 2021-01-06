// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/native_file_system_directory_handle_impl.h"

#include "base/i18n/file_util_icu.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/native_file_system_error.h"
#include "content/browser/file_system_access/native_file_system_transfer_token_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

using blink::mojom::FileSystemAccessEntry;
using blink::mojom::FileSystemAccessEntryPtr;
using blink::mojom::FileSystemAccessHandle;
using blink::mojom::FileSystemAccessStatus;
using blink::mojom::FileSystemAccessTransferToken;
using storage::FileSystemOperationRunner;

namespace content {

using HandleType = NativeFileSystemPermissionContext::HandleType;

NativeFileSystemDirectoryHandleImpl::NativeFileSystemDirectoryHandleImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : NativeFileSystemHandleBase(manager, context, url, handle_state) {}

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
  blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  if (get_child_url_result->status != FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result),
                            mojo::NullRemote());
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                FileSystemAccessStatus::kPermissionDenied),
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
        base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                          GetFileCallback callback) {
          std::move(callback).Run(std::move(result), mojo::NullRemote());
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
  blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  if (get_child_url_result->status != FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result),
                            mojo::NullRemote());
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                FileSystemAccessStatus::kPermissionDenied),
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
        base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                          GetDirectoryCallback callback) {
          std::move(callback).Run(std::move(result), mojo::NullRemote());
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
    mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<
      mojo::Remote<blink::mojom::FileSystemAccessDirectoryEntriesListener>,
      base::OnTaskRunnerDeleter>
      listener(
          new mojo::Remote<
              blink::mojom::FileSystemAccessDirectoryEntriesListener>(
              std::move(pending_listener)),
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  listener->reset_on_disconnect();

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    (*listener)->DidReadDirectory(
        native_file_system_error::FromStatus(
            FileSystemAccessStatus::kPermissionDenied),
        {}, false);
    return;
  }

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
  blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
      GetChildURL(basename, &child_url);
  if (get_child_url_result->status != FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(get_child_url_result));
    return;
  }

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemDirectoryHandleImpl::RemoveEntryImpl,
                     weak_factory_.GetWeakPtr(), child_url, recurse),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        RemoveEntryCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}
void NativeFileSystemDirectoryHandleImpl::Resolve(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        possible_child,
    ResolveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->ResolveTransferToken(
      std::move(possible_child),
      base::BindOnce(&NativeFileSystemDirectoryHandleImpl::ResolveImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NativeFileSystemDirectoryHandleImpl::ResolveImpl(
    ResolveCallback callback,
    NativeFileSystemTransferTokenImpl* possible_child) {
  if (!possible_child) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed),
        base::nullopt);
    return;
  }

  const storage::FileSystemURL& parent_url = url();
  const storage::FileSystemURL& child_url = possible_child->url();

  // If two URLs are of a different type they are definitely not related.
  if (parent_url.type() != child_url.type()) {
    std::move(callback).Run(native_file_system_error::Ok(), base::nullopt);
    return;
  }

  // Otherwise compare path.
  const base::FilePath& parent_path = parent_url.path();
  const base::FilePath& child_path = child_url.path();

  // Same path, so return empty array if child is also a directory.
  if (parent_path == child_path) {
    std::move(callback).Run(
        native_file_system_error::Ok(),
        possible_child->type() == HandleType::kDirectory
            ? base::make_optional(std::vector<std::string>())
            : base::nullopt);
    return;
  }

  // Now figure out relative path, if any.
  base::FilePath relative_path;
  if (parent_path.empty()) {
    // The root of a sandboxed file system will have an empty path. In that
    // case the child path is already the relative path.
    relative_path = child_path;
  } else if (!parent_path.AppendRelativePath(child_path, &relative_path)) {
    std::move(callback).Run(native_file_system_error::Ok(), base::nullopt);
    return;
  }

  std::vector<base::FilePath::StringType> components;
  relative_path.GetComponents(&components);
#if defined(OS_WIN)
  std::vector<std::string> result;
  result.reserve(components.size());
  for (const auto& component : components) {
    result.push_back(base::UTF16ToUTF8(component));
  }
  std::move(callback).Run(native_file_system_error::Ok(), std::move(result));
#else
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(components));
#endif
}

void NativeFileSystemDirectoryHandleImpl::Transfer(
    mojo::PendingReceiver<FileSystemAccessTransferToken> token) {
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
    mojo::Remote<blink::mojom::FileSystemAccessDirectoryEntriesListener>*
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

  std::vector<FileSystemAccessEntryPtr> entries;
  for (const auto& entry : file_list) {
    std::string basename = storage::FilePathToString(entry.name);

    storage::FileSystemURL child_url;
    blink::mojom::FileSystemAccessErrorPtr get_child_url_result =
        GetChildURL(basename, &child_url);

    // Skip any entries with names that aren't allowed to be accessed by
    // this API, such as files with disallowed characters in their names.
    if (get_child_url_result->status != FileSystemAccessStatus::kOk)
      continue;

    entries.push_back(
        CreateEntry(basename, child_url,
                    entry.type == filesystem::mojom::FsFileType::DIRECTORY
                        ? HandleType::kDirectory
                        : HandleType::kFile));
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

namespace {

// Returns whether the specified extension receives special handling by the
// Windows shell.
bool IsShellIntegratedExtension(const base::FilePath::StringType& extension) {
  base::FilePath::StringType extension_lower = base::ToLowerASCII(extension);

  // .lnk files may be used to execute arbitrary code (see
  // https://nvd.nist.gov/vuln/detail/CVE-2010-2568).
  if (extension_lower == FILE_PATH_LITERAL("lnk"))
    return true;

  // Setting a file's extension to a CLSID may conceal its actual file type on
  // some Windows versions (see https://nvd.nist.gov/vuln/detail/CVE-2004-0420).
  if (!extension_lower.empty() &&
      (extension_lower.front() == FILE_PATH_LITERAL('{')) &&
      (extension_lower.back() == FILE_PATH_LITERAL('}')))
    return true;
  return false;
}

}  // namespace

// static
bool NativeFileSystemDirectoryHandleImpl::IsSafePathComponent(
    const std::string& name) {
  // This method is similar to net::IsSafePortablePathComponent, with a few
  // notable differences where the net version does not consider names safe
  // while here we do want to allow them. These cases are:
  //  - Names starting with a '.'. These would be hidden files in most file
  //    managers, but are something we explicitly want to support for the
  //    File System Access API, for names like .git.
  //  - Names that end in '.local'. For downloads writing to such files is
  //    dangerous since it might modify what code is executed when an executable
  //    is ran from the same directory. For the File System Access API this
  //    isn't really a problem though, since if a website can write to a .local
  //    file via a FileSystemDirectoryHandle they can also just modify the
  //    executables in the directory directly.
  //
  // TODO(https://crbug.com/1154757): Unify this with
  // net::IsSafePortablePathComponent, with the result probably ending up in
  // base/i18n/file_util_icu.h.

  const base::FilePath component = storage::StringToFilePath(name);
  // Empty names, or names that contain path separators are invalid.
  if (component.empty() || component != component.BaseName() ||
      component != component.StripTrailingSeparators()) {
    return false;
  }

  base::string16 component16;
#if defined(OS_WIN)
  component16.assign(component.value().begin(), component.value().end());
#else
  std::string component8 = component.AsUTF8Unsafe();
  if (!base::UTF8ToUTF16(component8.c_str(), component8.size(), &component16))
    return false;
#endif
  // base::i18n::IsFilenameLegal blocks names that start with '.', so strip out
  // a leading '.' before passing it to that method.
  // TODO(mek): Consider making IsFilenameLegal more flexible to support this
  // use case.
  if (component16[0] == '.')
    component16 = component16.substr(1);
  if (!base::i18n::IsFilenameLegal(component16))
    return false;

  base::FilePath::StringType extension = component.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.
  if (IsShellIntegratedExtension(extension))
    return false;

  if (base::TrimString(component.value(), FILE_PATH_LITERAL("."),
                       base::TRIM_TRAILING) != component.value()) {
    return false;
  }

  if (net::IsReservedNameOnWindows(component.value()))
    return false;

  return true;
}

blink::mojom::FileSystemAccessErrorPtr
NativeFileSystemDirectoryHandleImpl::GetChildURL(
    const std::string& basename,
    storage::FileSystemURL* result) {
  if (!IsSafePathComponent(basename)) {
    return native_file_system_error::FromStatus(
        FileSystemAccessStatus::kInvalidArgument, "Name is not allowed.");
  }

  const storage::FileSystemURL parent = url();
  *result = file_system_context()->CreateCrackedFileSystemURL(
      parent.origin(), parent.mount_type(),
      parent.virtual_path().Append(base::FilePath::FromUTF8Unsafe(basename)));
  return native_file_system_error::Ok();
}

FileSystemAccessEntryPtr NativeFileSystemDirectoryHandleImpl::CreateEntry(
    const std::string& basename,
    const storage::FileSystemURL& url,
    HandleType handle_type) {
  if (handle_type == HandleType::kDirectory) {
    return FileSystemAccessEntry::New(
        FileSystemAccessHandle::NewDirectory(
            manager()->CreateDirectoryHandle(context(), url, handle_state())),
        basename);
  }
  return FileSystemAccessEntry::New(
      FileSystemAccessHandle::NewFile(
          manager()->CreateFileHandle(context(), url, handle_state())),
      basename);
}

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemDirectoryHandleImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
