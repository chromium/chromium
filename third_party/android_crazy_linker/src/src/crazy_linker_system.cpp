// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_system.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "crazy_linker_util.h"

// Note: unit-testing support files are in crazy_linker_files_mock.cpp

namespace crazy {

String MakeDirectoryPath(const char* parent) {
  return MakeDirectoryPath(parent, ::strlen(parent));
}

String MakeDirectoryPath(const char* parent, size_t parent_len) {
  if (parent_len == 0) {
    // Special case for empty inputs.
    return String("./");
  }
  String result(parent);
  if (parent_len > 0 && parent[parent_len - 1] != '/') {
    result += '/';
  }
  return result;
}

String MakeAbsolutePathFrom(const char* path) {
  return MakeAbsolutePathFrom(path, ::strlen(path));
}

String MakeAbsolutePathFrom(const char* path, size_t path_len) {
  if (path[0] == '/') {
    return String(path, path_len);
  } else {
    String cur_dir = GetCurrentDirectory();
    String result = MakeDirectoryPath(cur_dir.c_str(), cur_dir.size());
    result.Append(path, path_len);
    return result;
  }
}

bool IsSystemLibraryPath(const char* lib_path) {
  static const char* kSystemPrefixes[] = {
#ifdef __ANDROID__
      // From recent Android linker sources ($AOSP/bionic/linker/linker.cpp).
      "/system/lib64/", "/odm/lib64/", "/vendor/lib64/",
      "/data/asan/system/lib64/", "/data/asan/odm/lib64/",
      "/data/asan/vendor/lib64/",
      // It's ok to mix 32-bit and 64-bit paths in the same list here.
      "/system/lib/", "/odm/lib/", "/vendor/lib/", "/data/asan/system/lib/",
      "/data/asan/odm/lib/", "/data/asan/vendor/lib/",
#else
      // Typical system library directories for Linux systems.
      "/lib/",       "/lib32/",      "/lib64/",
      "/libx32/",    "/usr/lib/",    "/usr/lib32/",
      "/usr/lib64/", "/usr/libx32/", "/usr/local/lib/",
#endif
  };
  size_t lib_path_len = ::strlen(lib_path);
  for (const char* prefix : kSystemPrefixes) {
    size_t prefix_len = ::strlen(prefix);
    if (prefix_len < lib_path_len && !::memcmp(prefix, lib_path, prefix_len))
      return true;
  }
  return false;
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
  if (this != &other) {
    if (fd_ != kEmptyFD) {
      DoClose(fd_);
    }
    fd_ = other.fd_;
    other.fd_ = kEmptyFD;
  }
  return *this;
}

}  // namespace crazy

#ifndef UNIT_TEST

namespace crazy {

ssize_t FileDescriptor::Read(void* buffer, size_t buffer_size) {
  return TEMP_FAILURE_RETRY(::read(fd_, buffer, buffer_size));
}

off_t FileDescriptor::SeekTo(off_t offset) {
  return ::lseek(fd_, offset, SEEK_SET);
}

void* FileDescriptor::Map(void* address,
                          size_t length,
                          int prot,
                          int flags,
                          off_t offset) {
  void* mem = ::mmap(address, length, prot, flags, fd_, offset);
  return (mem == MAP_FAILED) ? nullptr : mem;
}

int64_t FileDescriptor::GetFileSize() const {
  struct stat stat_buf;
  if (fstat(fd_, &stat_buf) == -1) {
    return -1;
  }
  // |st_size| is an off_t which is always signed, but can be 32-bit or
  // 64-bit depending on the platform. Always convert to a signed 64-bit
  // to ensure the client always deal properly with both cases.
  return static_cast<int64_t>(stat_buf.st_size);
}

// static
int FileDescriptor::DoOpenReadOnly(const char* path) {
  return TEMP_FAILURE_RETRY(::open(path, O_RDONLY));
}

// static
int FileDescriptor::DoOpenReadWrite(const char* path) {
  return TEMP_FAILURE_RETRY(::open(path, O_RDWR));
}

// static
void FileDescriptor::DoClose(int fd) {
  int old_errno = errno;
  // SUBTLE: Do not loop when close() returns EINTR. On Linux, this simply
  // means that a corresponding flush operation failed, but the file
  // descriptor will always be closed anyway.
  //
  // Other platforms have different behavior: e.g. on OS X, this could be
  // the result of an interrupt, and there is no reliable way to know
  // whether the fd was closed or not on exit :-(
  (void)close(fd);
  errno = old_errno;
}

const char* GetEnv(const char* var_name) { return ::getenv(var_name); }

String GetCurrentDirectory() {
  String result;
  size_t capacity = 128;
  for (;;) {
    result.Resize(capacity);
    if (getcwd(&result[0], capacity))
      break;
    capacity *= 2;
  }
  return result;
}

bool PathExists(const char* path) {
  struct stat st;
  if (TEMP_FAILURE_RETRY(stat(path, &st)) < 0)
    return false;

  return S_ISREG(st.st_mode) || S_ISDIR(st.st_mode);
}

bool PathIsFile(const char* path) {
  struct stat st;
  if (TEMP_FAILURE_RETRY(stat(path, &st)) < 0)
    return false;

  return S_ISREG(st.st_mode);
}

}  // namespace crazy

#if !defined(CRAZY_LINKER_ENABLE_FUZZING)

// Custom implementation of new and malloc, this prevents dragging
// the libc++ implementation, which drags exception-related machine
// code that is not needed here. This helps reduce the size of the
// final binary considerably.

// IMPORTANT: These symbols are not exported by the crazy linker, thus this
//            does not affect the libraries that it will load, only the
//            linker binary itself!
//
void* operator new(size_t size) {
  void* ptr = ::malloc(size);
  if (ptr != nullptr)
    return ptr;

  // Don't assume it is possible to call any C library function like
  // snprintf() here, since it might allocate heap memory and crash at
  // runtime. Hence our fatal message does not contain the number of
  // bytes requested by the allocation.
  static const char kFatalMessage[] = "Out of memory!";
#ifdef __ANDROID__
  __android_log_write(ANDROID_LOG_FATAL, "crazy_linker", kFatalMessage);
#else
  ::write(STDERR_FILENO, kFatalMessage, sizeof(kFatalMessage) - 1);
#endif
  _exit(1);
#if defined(__GNUC__)
  __builtin_unreachable();
#endif

  // NOTE: Adding a 'return nullptr' here will make the compiler error
  // with a message stating that 'operator new(size_t)' is not allowed
  // to return nullptr.
  //
  // Indeed, an new expression like 'new T' shall never return nullptr,
  // according to the C++ specification, and an optimizing compiler will gladly
  // remove any null-checks after them (something the Fuschsia team had to
  // learn the hard way when writing their kernel in C++). What is meant here
  // is something like:
  //
  //   Foo* foo = new Foo(10);
  //   if (!foo) {                             <-- entire check and branch
  //      ... Handle out-of-memory condition.  <-- removed by an optimizing
  //   }                                       <-- compiler.
  //
  // Note that some C++ library implementations (e.g. recent libc++) recognize
  // when they are compiled with -fno-exceptions and provide a simpler version
  // of operator new that can return nullptr. However, it is very hard to
  // guarantee at build time that this code is linked against such a version
  // of the runtime. Moreoever, technically disabling exceptions is completely
  // out-of-spec regarding the C++ language, and what the compiler is allowed
  // to do in this case is mostly implementation-defined, so better be safe
  // than sorry here.
  //
  // C++ provides a non-throwing new expression that can return a nullptr
  // value, but it must be written as 'new (std::nothrow) T' instead of
  // 'new T', and thus nobody uses this. This ends up calling
  // 'operator new(size_t, const std::nothrow_t&)' which is not implemented
  // here.
}

void* operator new[](size_t size) {
  return operator new(size);
}

void operator delete(void* ptr) {
  // The compiler-generated code already checked that |ptr != nullptr|
  // so don't to it a second time.
  ::free(ptr);
}

void operator delete[](void* ptr) {
  ::free(ptr);
}

#endif  // !CRAZY_LINKER_ENABLE_FUZZING

#endif  // !UNIT_TEST
