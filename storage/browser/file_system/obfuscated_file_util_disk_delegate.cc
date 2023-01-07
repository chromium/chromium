// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/obfuscated_file_util_disk_delegate.h"

#include "base/files/file_util.h"
#include "storage/browser/file_system/native_file_util.h"

namespace storage {

ObfuscatedFileUtilDiskDelegate::ObfuscatedFileUtilDiskDelegate() = default;

ObfuscatedFileUtilDiskDelegate::~ObfuscatedFileUtilDiskDelegate() = default;

bool ObfuscatedFileUtilDiskDelegate::DirectoryExists(
    const base::FilePath& path) {
  return base::DirectoryExists(path);
}

size_t ObfuscatedFileUtilDiskDelegate::ComputeDirectorySize(
    const base::FilePath& path) {
  return base::ComputeDirectorySize(path);
}

bool ObfuscatedFileUtilDiskDelegate::DeleteFileOrDirectory(
    const base::FilePath& path,
    bool recursive) {
  if (!recursive)
    return base::DeleteFile(path);
  return base::DeletePathRecursively(path);
}

bool ObfuscatedFileUtilDiskDelegate::IsLink(const base::FilePath& file_path) {
  return base::IsLink(file_path);
}

bool ObfuscatedFileUtilDiskDelegate::PathExists(const base::FilePath& path) {
  return base::PathExists(path);
}

NativeFileUtil::CopyOrMoveMode
ObfuscatedFileUtilDiskDelegate::CopyOrMoveModeForDestination(
    const FileSystemURL& dest_url,
    bool copy) {
  return NativeFileUtil::CopyOrMoveModeForDestination(dest_url, copy);
}

base::File ObfuscatedFileUtilDiskDelegate::CreateOrOpen(
    const base::FilePath& path,
    uint32_t file_flags) {
  return NativeFileUtil::CreateOrOpen(path, file_flags);
}

base::File::Error ObfuscatedFileUtilDiskDelegate::EnsureFileExists(
    const base::FilePath& path,
    bool* created) {
  return NativeFileUtil::EnsureFileExists(path, created);
}
base::File::Error ObfuscatedFileUtilDiskDelegate::CreateDirectory(
    const base::FilePath& path,
    bool exclusive,
    bool recursive) {
  return NativeFileUtil::CreateDirectory(path, exclusive, recursive);
}

base::File::Error ObfuscatedFileUtilDiskDelegate::GetFileInfo(
    const base::FilePath& path,
    base::File::Info* file_info) {
  return NativeFileUtil::GetFileInfo(path, file_info);
}
base::File::Error ObfuscatedFileUtilDiskDelegate::Touch(
    const base::FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  return NativeFileUtil::Touch(path, last_access_time, last_modified_time);
}

base::File::Error ObfuscatedFileUtilDiskDelegate::Truncate(
    const base::FilePath& path,
    int64_t length) {
  return NativeFileUtil::Truncate(path, length);
}

base::File::Error ObfuscatedFileUtilDiskDelegate::CopyOrMoveFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    FileSystemOperation::CopyOrMoveOptionSet options,
    NativeFileUtil::CopyOrMoveMode mode) {
  return NativeFileUtil::CopyOrMoveFile(src_path, dest_path, options, mode);
}

base::File::Error ObfuscatedFileUtilDiskDelegate::CopyInForeignFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    FileSystemOperation::CopyOrMoveOptionSet options,
    NativeFileUtil::CopyOrMoveMode mode) {
  return NativeFileUtil::CopyOrMoveFile(src_path, dest_path, options, mode);
}

base::File::Error ObfuscatedFileUtilDiskDelegate::DeleteFile(
    const base::FilePath& path) {
  return NativeFileUtil::DeleteFile(path);
}

}  // namespace storage
