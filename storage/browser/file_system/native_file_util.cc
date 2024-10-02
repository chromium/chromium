// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/native_file_util.h"

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_mount_option.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "windows.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
#include <grp.h>
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace storage {

namespace {

// Sets permissions on directory at |dir_path| based on the target platform.
// Returns true on success, or false otherwise.
//
// TODO(benchan): Find a better place outside webkit to host this function.
bool SetPlatformSpecificDirectoryPermissions(const base::FilePath& dir_path) {
#if BUILDFLAG(IS_CHROMEOS)
  // System daemons on ChromeOS may run as a user different than the Chrome
  // process but need to access files under the directories created here.
  // Because of that, grant the execute permission on the created directory
  // to group and other users. Also chronos-access group should have read
  // access to the directory.
  if (HANDLE_EINTR(chmod(dir_path.value().c_str(),
                         S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH)) != 0) {
    return false;
  }
  struct group grp, *result = nullptr;
  std::vector<char> buffer(16384);
  // HANDLE_EINTR is not suitable for use with getgrnam_r, as it returns the
  // error rather than setting errno.
  while (getgrnam_r("chronos-access", &grp, buffer.data(), buffer.size(),
                    &result) == EINTR) {
  }
  // Ignoring as the group might not exist in tests.
  if (result &&
      HANDLE_EINTR(chown(dir_path.value().c_str(), -1, grp.gr_gid)) != 0) {
    return false;
  }
#endif
  // Keep the directory permissions unchanged on non-Chrome OS platforms.
  return true;
}

// Copies a file |from| to |to|, and ensure the written content is synced to
// the disk. This is essentially base::CopyFile followed by fsync().
bool CopyFileAndSync(const base::FilePath& from, const base::FilePath& to) {
  base::File infile(from, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!infile.IsValid()) {
    return false;
  }

  base::File outfile(to,
                     base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!outfile.IsValid()) {
    return false;
  }

  const int kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);

  for (;;) {
    std::optional<size_t> bytes_read =
        infile.ReadAtCurrentPos(base::as_writable_byte_span(buffer));
    if (!bytes_read.has_value()) {
      return false;
    }
    if (bytes_read.value() == 0) {
      break;
    }
    auto span_to_write = base::as_byte_span(buffer).first(bytes_read.value());
    while (!span_to_write.empty()) {
      std::optional<size_t> bytes_written_partial =
          outfile.WriteAtCurrentPos(span_to_write);
      if (!bytes_written_partial.has_value()) {
        return false;
      }
      span_to_write = span_to_write.subspan(bytes_written_partial.value());
    }
  }

  return outfile.Flush();
}

}  // namespace

using base::PlatformFile;

class NativeFileEnumerator : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  NativeFileEnumerator(const base::FilePath& root_path,
                       bool recursive,
                       int file_type)
      : file_enum_(root_path, recursive, file_type) {}

  ~NativeFileEnumerator() override = default;

  base::FilePath Next() override;
  base::FilePath GetName() override;
  int64_t Size() override;
  base::Time LastModifiedTime() override;
  bool IsDirectory() override;

 private:
  base::FileEnumerator file_enum_;
  base::FileEnumerator::FileInfo file_util_info_;
};

base::FilePath NativeFileEnumerator::Next() {
  base::FilePath rv = file_enum_.Next();
  if (!rv.empty())
    file_util_info_ = file_enum_.GetInfo();
  return rv;
}

base::FilePath NativeFileEnumerator::GetName() {
  return file_util_info_.GetName();
}

int64_t NativeFileEnumerator::Size() {
  return file_util_info_.GetSize();
}

base::Time NativeFileEnumerator::LastModifiedTime() {
  return file_util_info_.GetLastModifiedTime();
}

bool NativeFileEnumerator::IsDirectory() {
  return file_util_info_.IsDirectory();
}

NativeFileUtil::CopyOrMoveMode NativeFileUtil::CopyOrMoveModeForDestination(
    const FileSystemURL& dest_url,
    bool copy) {
  if (copy) {
    return dest_url.mount_option().flush_policy() ==
                   FlushPolicy::FLUSH_ON_COMPLETION
               ? COPY_SYNC
               : COPY_NOSYNC;
  }
  return MOVE;
}

base::File NativeFileUtil::CreateOrOpen(const base::FilePath& path,
                                        uint32_t file_flags) {
  if (!base::DirectoryExists(path.DirName())) {
    // If its parent does not exist, should return NOT_FOUND error.
    return base::File(base::File::FILE_ERROR_NOT_FOUND);
  }

  // TODO(rvargas): Check |file_flags| instead. See bug 356358.
  if (base::DirectoryExists(path))
    return base::File(base::File::FILE_ERROR_NOT_A_FILE);

  // This file might be passed to an untrusted process.
  file_flags = base::File::AddFlagsForPassingToUntrustedProcess(file_flags);

  return base::File(path, file_flags);
}

base::File::Error NativeFileUtil::EnsureFileExists(const base::FilePath& path,
                                                   bool* created) {
#if BUILDFLAG(IS_ANDROID)
  if (path.IsContentUri()) {
    if (base::PathExists(path)) {
      if (created) {
        *created = false;
      }
      return base::File::FILE_OK;
    }
    base::File file(path,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (file.IsValid()) {
      if (created) {
        *created = true;
      }
      return base::File::FILE_OK;
    }
    if (created) {
      *created = false;
    }
    return base::File::FILE_ERROR_NOT_FOUND;
  }
#endif
  if (!base::DirectoryExists(path.DirName()))
    // If its parent does not exist, should return NOT_FOUND error.
    return base::File::FILE_ERROR_NOT_FOUND;

  // If |path| is a directory, return an error.
  if (base::DirectoryExists(path))
    return base::File::FILE_ERROR_NOT_A_FILE;

  // Tries to create the |path| exclusively.  This should fail
  // with base::File::FILE_ERROR_EXISTS if the path already exists.
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_READ);

  if (file.IsValid()) {
    if (created)
      *created = file.created();
    return base::File::FILE_OK;
  }

  base::File::Error error_code = file.error_details();
  if (error_code == base::File::FILE_ERROR_EXISTS) {
    // Make sure created_ is false.
    if (created)
      *created = false;
    error_code = base::File::FILE_OK;
  }
  return error_code;
}

base::File::Error NativeFileUtil::CreateDirectory(const base::FilePath& path,
                                                  bool exclusive,
                                                  bool recursive) {
  // If parent dir of file doesn't exist.
  if (!recursive && !base::PathExists(path.DirName()))
    return base::File::FILE_ERROR_NOT_FOUND;

  bool path_exists = base::PathExists(path);
  if (exclusive && path_exists)
    return base::File::FILE_ERROR_EXISTS;

  // If file exists at the path.
  if (path_exists && !base::DirectoryExists(path))
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;

  if (!base::CreateDirectory(path))
    return base::File::FILE_ERROR_FAILED;

  if (!SetPlatformSpecificDirectoryPermissions(path)) {
    // Since some file systems don't support permission setting, we do not treat
    // an error from the function as the failure of copying. Just log it.
    LOG(WARNING) << "Setting directory permission failed: "
                 << path.AsUTF8Unsafe();
  }

  return base::File::FILE_OK;
}

base::File::Error NativeFileUtil::GetFileInfo(const base::FilePath& path,
                                              base::File::Info* file_info) {
  if (!base::PathExists(path))
    return base::File::FILE_ERROR_NOT_FOUND;

  if (!base::GetFileInfo(path, file_info))
    return base::File::FILE_ERROR_FAILED;
  return base::File::FILE_OK;
}

std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator>
NativeFileUtil::CreateFileEnumerator(const base::FilePath& root_path,
                                     bool recursive) {
  return std::make_unique<NativeFileEnumerator>(
      root_path, recursive,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
}

base::File::Error NativeFileUtil::Touch(const base::FilePath& path,
                                        const base::Time& last_access_time,
                                        const base::Time& last_modified_time) {
  if (!base::TouchFile(path, last_access_time, last_modified_time))
    return base::File::FILE_ERROR_FAILED;
  return base::File::FILE_OK;
}

base::File::Error NativeFileUtil::Truncate(const base::FilePath& path,
                                           int64_t length) {
#if BUILDFLAG(IS_ANDROID)
  if (path.IsContentUri()) {
    if (length != 0) {
      return base::File::FILE_ERROR_FAILED;
    }
    base::File file(path,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    return file.error_details();
  }
#endif
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return file.error_details();

  if (!file.SetLength(length))
    return base::File::FILE_ERROR_FAILED;

  return base::File::FILE_OK;
}

bool NativeFileUtil::PathExists(const base::FilePath& path) {
  return base::PathExists(path);
}

bool NativeFileUtil::DirectoryExists(const base::FilePath& path) {
  return base::DirectoryExists(path);
}

base::File::Error NativeFileUtil::CopyOrMoveFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    FileSystemOperation::CopyOrMoveOptionSet options,
    CopyOrMoveMode mode) {
  base::File::Info info;
  base::File::Error error = NativeFileUtil::GetFileInfo(src_path, &info);
  if (error != base::File::FILE_OK)
    return error;
  if (info.is_directory && mode != MOVE)
    return base::File::FILE_ERROR_NOT_A_FILE;
  bool src_is_directory = info.is_directory;
  base::Time last_modified = info.last_modified;

  error = NativeFileUtil::GetFileInfo(dest_path, &info);
  if (error != base::File::FILE_OK && error != base::File::FILE_ERROR_NOT_FOUND)
    return error;
  if (error == base::File::FILE_OK) {
    if (info.is_directory != src_is_directory)
      return base::File::FILE_ERROR_INVALID_OPERATION;
#if BUILDFLAG(IS_WIN)
    // Overwriting an empty directory with another directory isn't supported
    // natively on Windows, so treat this an unsupported. A higher layer is
    // responsible for handling it.
    if (info.is_directory)
      return base::File::FILE_ERROR_NOT_A_FILE;
#endif
  }
  if (error == base::File::FILE_ERROR_NOT_FOUND) {
    error = NativeFileUtil::GetFileInfo(dest_path.DirName(), &info);
    if (error != base::File::FILE_OK)
      return error;
    if (!info.is_directory)
      return base::File::FILE_ERROR_NOT_FOUND;
  }

  // Cache permissions of dest file before copy/move overwrites the file.
  bool should_retain_file_permissions = false;
#if BUILDFLAG(IS_POSIX)
  int dest_mode;
  if (options.Has(FileSystemOperation::CopyOrMoveOption::
                      kPreserveDestinationPermissions)) {
    // Will be false if the destination file doesn't exist.
    should_retain_file_permissions =
        base::GetPosixFilePermissions(dest_path, &dest_mode);
  }
#elif BUILDFLAG(IS_WIN)
  DWORD dest_attributes;
  if (options.Has(FileSystemOperation::CopyOrMoveOption::
                      kPreserveDestinationPermissions)) {
    dest_attributes = ::GetFileAttributes(dest_path.value().c_str());
    should_retain_file_permissions = dest_attributes != INVALID_FILE_ATTRIBUTES;
  }
#endif  // BUILDFLAG(IS_POSIX)

  switch (mode) {
    case COPY_NOSYNC:
      if (!base::CopyFile(src_path, dest_path))
        return base::File::FILE_ERROR_FAILED;
      break;
    case COPY_SYNC:
      if (!CopyFileAndSync(src_path, dest_path))
        return base::File::FILE_ERROR_FAILED;
      break;
    case MOVE:
      if (!base::Move(src_path, dest_path))
        return base::File::FILE_ERROR_FAILED;
      break;
  }

  // Preserve the last modified time. Do not return error here even if
  // the setting is failed, because the copy itself is successfully done.
  if (options.Has(
          FileSystemOperation::CopyOrMoveOption::kPreserveLastModified)) {
    base::TouchFile(dest_path, last_modified, last_modified);
  }

  if (should_retain_file_permissions) {
#if BUILDFLAG(IS_POSIX)
    base::SetPosixFilePermissions(dest_path, dest_mode);
#elif BUILDFLAG(IS_WIN)
    ::SetFileAttributes(dest_path.value().c_str(), dest_attributes);
#endif  // BUILDFLAG(IS_POSIX)
  }

  return base::File::FILE_OK;
}

base::File::Error NativeFileUtil::DeleteFile(const base::FilePath& path) {
  if (!base::PathExists(path))
    return base::File::FILE_ERROR_NOT_FOUND;
  if (base::DirectoryExists(path))
    return base::File::FILE_ERROR_NOT_A_FILE;
  if (!base::DeleteFile(path))
    return base::File::FILE_ERROR_FAILED;
  return base::File::FILE_OK;
}

base::File::Error NativeFileUtil::DeleteDirectory(const base::FilePath& path) {
  if (!base::PathExists(path))
    return base::File::FILE_ERROR_NOT_FOUND;
  if (!base::DirectoryExists(path))
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;
  if (!base::IsDirectoryEmpty(path))
    return base::File::FILE_ERROR_NOT_EMPTY;
  if (!base::DeleteFile(path))
    return base::File::FILE_ERROR_FAILED;
  return base::File::FILE_OK;
}

}  // namespace storage
