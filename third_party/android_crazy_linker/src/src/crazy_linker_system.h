// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_SYSTEM_H
#define CRAZY_LINKER_SYSTEM_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "crazy_linker_macros.h"
#include "crazy_linker_util.h"  // for String

// System abstraction used by the crazy linker.
// This is used to make unit testing easier without using tons of
// dependency injection in the rest of the code base.
//
// In a nutshell: in a normal build, this will wrap normal open() / read()
// calls. During unit testing, everything is mocked, see
// crazy_linker_system_mock.cpp

namespace crazy {

enum FileOpenMode {
  FILE_OPEN_READ_ONLY = 0,
  FILE_OPEN_READ_WRITE,
  FILE_OPEN_WRITE
};

// Scoping wrapper for a platform file descriptor.
// The descriptor is closed on destruction, unless Release() is called.
//
// IMPORTANT NOTE: The purpose of this file is only to provide a way to mock
// the file system during unit testing. There are simple cases where it is
// better to use direct syscalls (e.g. Ashmem region file descriptors require
// specific opening a non-mockable location (/dev/ashmem) as well as ioctl()
// calls not covered here).
class FileDescriptor {
 public:
  using HandleType = int;

  static constexpr HandleType kEmptyFD = -1;

  constexpr FileDescriptor() = default;

  FileDescriptor(const char* path) : fd_(DoOpenReadOnly(path)) {}

  ~FileDescriptor() { Close(); }

  CRAZY_DISALLOW_COPY_OPERATIONS(FileDescriptor)

  // Move operations are allowed.
  FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
    other.fd_ = kEmptyFD;
  }

  FileDescriptor& operator=(FileDescriptor&& other) noexcept;

  // Returns true if the descriptor is valid.
  bool IsOk() const { return fd_ != kEmptyFD; }

  // Return the value of the platform file descriptor.
  HandleType Get() const { return fd_; }

  // Close the current descriptor, and try to open a file read-only.
  // Return true on success, false/errno on failure.
  bool OpenReadOnly(const char* path) {
    Close();
    fd_ = DoOpenReadOnly(path);
    return IsOk();
  }

  // Close the current descriptor, then try to open a file read-write.
  // Return true on success, false/errno on failure.
  bool OpenReadWrite(const char* path) {
    Close();
    fd_ = DoOpenReadWrite(path);
    return IsOk();
  }

  // Try to read |buffer_size| bytes into |buffer|. On success, return the
  // number of bytes that were read, or 0 for EOF, or -1/errno for I/O
  // error.
  ssize_t Read(void* buffer, size_t buffer_size);

  // Try to read exactly |buffer_size| bytes into |buffer|. Return true
  // on success, false/errno on failure.
  bool ReadFull(void* buffer, size_t buffer_size) {
    ssize_t ret = Read(buffer, buffer_size);
    return (ret >= 0 && static_cast<size_t>(ret) == buffer_size);
  }

  // Seek to a specific offset of the file. Return |offset| on success, or
  // -1/errno on error.
  off_t SeekTo(off_t offset);

  // Map the file into memory. Parameters must match the ::mmap() system call.
  // Return a new memory address on success, or nullptr on failure.
  void* Map(void* address,
            size_t length,
            int prot_flags,
            int flags,
            off_t offset);

  // Return the size in bytes of the corresponding file, or -1/errno if the
  // descriptor is invalid.
  int64_t GetFileSize() const;

  // Close the file descriptor if needed.
  void Close() {
    if (fd_ != kEmptyFD) {
      DoClose(fd_);
      fd_ = kEmptyFD;
    }
  }

  // Release ownership of the file descriptor. The caller becomes responsible
  // for closing the returned handle.
  HandleType Release() {
    HandleType ret = fd_;
    fd_ = kEmptyFD;
    return ret;
  }

 protected:
  explicit FileDescriptor(HandleType handle) : fd_(handle) {}

  static int DoOpenReadOnly(const char* path);
  static int DoOpenReadWrite(const char* path);
  static void DoClose(int fd);

  HandleType fd_ = kEmptyFD;
};

// Returns true iff a given file path exists.
bool PathExists(const char* path_name);

// Returns true iff a given path is a regular file (or link to a regular
// file).
bool PathIsFile(const char* path_name);

// Returns the current directory, as a string.
String GetCurrentDirectory();

// Convert |path| into a String, and appends a trailing directory separator
// if there isn't already one. NOTE: As a special case, if the input is empty,
// then "./" will be returned.
String MakeDirectoryPath(const char* path);
String MakeDirectoryPath(const char* path, size_t path_len);

// Convert |path| into an absolute path if necessary, always returns a new
// String instance as well.
String MakeAbsolutePathFrom(const char* path);

// Same, but for the [path..path + path_len) input string.
String MakeAbsolutePathFrom(const char* path, size_t path_len);

// Returns the value of a given environment variable.
const char* GetEnv(const char* var_name);

// Returns true iff |lib_path| corresponds to the path of a system library,
// which should always be loaded through the system linker, and not the
// crazy one. Note that |lib_path| must be an absolute path, not a relative
// one.
bool IsSystemLibraryPath(const char* lib_path);

}  // namespace crazy

#endif  // CRAZY_LINKER_SYSTEM_H
