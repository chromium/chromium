// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A set of common helper functions used by crazy_linker tests.
// IMPORTANT: ALL FUNCTIONS HERE ARE INLINED. This avoids adding a
// dependency on another source file for all tests that include this
// header.

#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <crazy_linker.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS // to get PRI and SCN in 32-bit inttypes.h
#endif
#include <inttypes.h>

namespace {

// Print an error message and exit the process.
// Message must be terminated by a newline.
inline void Panic(const char* fmt, ...) {
  va_list args;
  fprintf(stderr, "PANIC: ");
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(1);
}

// Print an error message, the errno message, then exit the process.
// Message must not be terminated by a newline.
inline void PanicErrno(const char* fmt, ...) {
  int old_errno = errno;
  va_list args;
  fprintf(stderr, "PANIC: ");
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, ": %s\n", strerror(old_errno));
  exit(1);
}

// Simple string class.
class String {
 public:
  String() : str_(NULL), len_(0) {}

  String(const String& other) { String(other.str_, other.len_); }

  String(const char* str) { String(str, strlen(str)); }

  String(const char* str, size_t len) : str_(NULL), len_(0) {
    Append(str, len);
  }

  ~String() {
    if (str_) {
      free(str_);
      str_ = NULL;
    }
  }

  String& operator+=(const char* str) {
    Append(str, strlen(str));
    return *this;
  }

  String& operator+=(const String& other) {
    Append(other.str_, other.len_);
    return *this;
  }

  String& operator+=(char ch) {
    Append(&ch, 1);
    return *this;
  }

  const char* c_str() const { return len_ ? str_ : ""; }
  char* ptr() { return str_; }
  size_t size() const { return len_; }

  void Append(const char* str, size_t len) {
    size_t old_len = len_;
    Resize(len_ + len);
    memcpy(str_ + old_len, str, len);
  }

  void Resize(size_t len) {
    str_ = reinterpret_cast<char*>(realloc(str_, len + 1));
    if (len > len_)
      memset(str_ + len_, '\0', len - len_);
    str_[len] = '\0';
    len_ = len;
  }

  void Format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Resize(128);
    for (;;) {
      va_list args2;
      va_copy(args2, args);
      int ret = vsnprintf(str_, len_ + 1, fmt, args2);
      va_end(args2);
      if (ret < static_cast<int>(len_ + 1))
        break;

      Resize(len_ * 2);
    }
  }

 private:
  char* str_;
  size_t len_;
};

// Helper class to create a temporary directory that gets deleted on scope exit,
// as well as all regular files it contains.
class TempDirectory {
 public:
  TempDirectory() {
    snprintf(path_, sizeof path_, "/data/local/tmp/temp-XXXXXX");
    if (!mkdtemp(path_))
      Panic("Could not create temporary directory name: %s\n", strerror(errno));
  }

  ~TempDirectory() {
    // Remove any file in this directory.
    DIR* d = opendir(path_);
    if (!d)
      Panic("Could not open directory %s: %s\n", strerror(errno));

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
        continue;
      String file_path;
      file_path.Format("%s/%s", path_, entry->d_name);
      if (unlink(file_path.c_str()) < 0)
        Panic("Could not remove %s: %s\n", file_path.c_str(), strerror(errno));
    }
    closedir(d);

    if (rmdir(path_) < 0)
      Panic("Could not remove dir %s: %s\n", path_, strerror(errno));
  }

  const char* path() const { return path_; }

 private:
  char path_[PATH_MAX];
};

// Scoped FILE* class. Always closed on destruction.
class ScopedFILE {
 public:
  ScopedFILE() : file_(NULL) {}

  ~ScopedFILE() {
    if (file_) {
      fclose(file_);
      file_ = NULL;
    }
  }

  void Open(const char* path, const char* mode) {
    file_ = fopen(path, mode);
    if (!file_)
      Panic("Could not open file for reading: %s: %s\n", path, strerror(errno));
  }

  FILE* file() const { return file_; }

 private:
  FILE* file_;
};

// Retrieve current executable path as a String.
inline String GetCurrentExecutable() {
  String path;
  path.Resize(PATH_MAX);
  ssize_t ret =
      TEMP_FAILURE_RETRY(readlink("/proc/self/exe", path.ptr(), path.size()));
  if (ret < 0)
    Panic("Could not read /proc/self/exe: %s\n", strerror(errno));

  return path;
}

// Retrieve current executable directory as a String.
inline String GetCurrentExecutableDirectory() {
  String path = GetCurrentExecutable();
  // Find basename.
  const char* p = reinterpret_cast<const char*>(strrchr(path.c_str(), '/'));
  if (p == NULL)
    Panic("Current executable does not have directory root?: %s\n",
          path.c_str());

  path.Resize(p - path.c_str());
  return path;
}

// Copy a file named |src_file_name| in directory |src_file_dir| into
// a file named |dst_file_name| in directory |dst_file_dir|. Panics on error.
inline void CopyFile(const char* src_file_name,
                     const char* src_file_dir,
                     const char* dst_file_name,
                     const char* dst_file_dir) {
  String src_path;
  src_path.Format("%s/%s", src_file_dir, src_file_name);

  ScopedFILE src_file;
  src_file.Open(src_path.c_str(), "rb");

  String dst_path;
  dst_path.Format("%s/%s", dst_file_dir, dst_file_name);
  ScopedFILE dst_file;
  dst_file.Open(dst_path.c_str(), "wb");

  char buffer[8192];
  for (;;) {
    size_t read = fread(buffer, 1, sizeof buffer, src_file.file());
    if (read > 0) {
      size_t written = fwrite(buffer, 1, read, dst_file.file());
      if (written != read)
        Panic("Wrote %d bytes instead of %d into %s\n",
              written,
              read,
              dst_path.c_str());
    }
    if (read < sizeof buffer)
      break;
  }
}

// Send a file descriptor |fd| through |socket|.
// Return 0 on success, -1/errno on failure.
inline int SendFd(int socket, int fd) {
  struct iovec iov;

  char buffer[1];
  buffer[0] = 0;

  iov.iov_base = buffer;
  iov.iov_len = 1;

  struct msghdr msg;
  struct cmsghdr* cmsg;
  char cms[CMSG_SPACE(sizeof(int))];

  ::memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = reinterpret_cast<caddr_t>(cms);
  msg.msg_controllen = CMSG_LEN(sizeof(int));

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  ::memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

  int ret = sendmsg(socket, &msg, 0);
  if (ret < 0)
    return -1;

  if (ret != (int)iov.iov_len) {
    errno = EIO;
    return -1;
  }

  return 0;
}

inline int ReceiveFd(int socket, int* fd) {
  char buffer[1];
  struct iovec iov;

  iov.iov_base = buffer;
  iov.iov_len = 1;

  struct msghdr msg;
  struct cmsghdr* cmsg;
  char cms[CMSG_SPACE(sizeof(int))];

  ::memset(&msg, 0, sizeof msg);
  msg.msg_name = 0;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  msg.msg_control = reinterpret_cast<caddr_t>(cms);
  msg.msg_controllen = sizeof(cms);

  int ret = recvmsg(socket, &msg, 0);
  if (ret < 0)
    return -1;
  if (ret == 0) {
    errno = EIO;
    return -1;
  }

  cmsg = CMSG_FIRSTHDR(&msg);
  ::memcpy(fd, CMSG_DATA(cmsg), sizeof(int));
  return 0;
}

// Check that there are exactly |expected_count| memory mappings in
// /proc/self/maps that point to a RELRO ashmem region.
inline void CheckRelroMaps(int expected_count) {
  printf("Checking for %d RELROs in /proc/self/maps\n", expected_count);

  FILE* file = fopen("/proc/self/maps", "rb");
  if (!file)
    Panic("Could not open /proc/self/maps (pid %d): %s\n",
          getpid(),
          strerror(errno));

  char line[512];
  int count_relros = 0;
  printf("proc/%d/maps:\n", getpid());
  while (fgets(line, sizeof line, file)) {
    if (strstr(line, "with_relro")) {
      // The supported library names are "lib<name>_with_relro.so".
      printf("%s", line);
      if (strstr(line, "/dev/ashmem/RELRO:")) {
        count_relros++;
        // Check that they are read-only mappings.
        if (!strstr(line, " r--"))
          Panic("Shared RELRO mapping is not readonly!\n");
        // Check that they can't be remapped read-write.
        uint64_t vma_start, vma_end;
        if (sscanf(line, "%" SCNx64 "-%" SCNx64, &vma_start, &vma_end) != 2)
          Panic("Could not parse VM address range!\n");
        int ret = ::mprotect(
            (void*)vma_start, vma_end - vma_start, PROT_READ | PROT_WRITE);
        if (ret == 0)
          Panic("Could remap shared RELRO as writable, should not happen!\n");

        if (errno != EACCES)
          Panic("remapping shared RELRO to writable failed with: %s\n",
                strerror(errno));
      }
    }
  }
  fclose(file);

  if (count_relros != expected_count)
    Panic(
        "Invalid shared RELRO sections in /proc/self/maps: %d"
        " (expected %d)\n",
        count_relros,
        expected_count);

  printf("RELRO count check ok!\n");
}

struct RelroInfo {
  size_t start;
  size_t size;
  int fd;
};

struct RelroLibrary {
  const char* name;
  crazy_library_t* library;
  RelroInfo relro;

  void Init(const char* name_str, crazy_context_t* context) {
    printf("Loading %s\n", name_str);
    name = name_str;
    if (!crazy_library_open(&this->library, name_str, context)) {
      Panic("Could not open %s: %s\n", name_str,
            crazy_context_get_error(context));
    }
  }

  void Close() { crazy_library_close(this->library); }

  void CreateSharedRelro(crazy_context_t* context, size_t load_address) {
    if (!crazy_library_create_shared_relro(this->library,
                                           context,
                                           load_address,
                                           &this->relro.start,
                                           &this->relro.size,
                                           &this->relro.fd)) {
      Panic("Could not create shared RELRO for %s: %s",
            this->name,
            crazy_context_get_error(context));
    }

    printf("Parent %s relro info relro_start=%p relro_size=%p relro_fd=%d\n",
           this->name,
           (void*)this->relro.start,
           (void*)this->relro.size,
           this->relro.fd);
  }

  void EnableSharedRelro(crazy_context_t* context) {
    CreateSharedRelro(context, 0);
    UseSharedRelro(context);
  }

  void SendRelroInfo(int fd) {
    if (SendFd(fd, this->relro.fd) < 0) {
      Panic("Could not send %s RELRO fd: %s", this->name, strerror(errno));
    }

    int ret =
        TEMP_FAILURE_RETRY(::write(fd, &this->relro, sizeof(this->relro)));
    if (ret != static_cast<int>(sizeof(this->relro))) {
      Panic("Parent could not send %s RELRO info: %s",
            this->name,
            strerror(errno));
    }
  }

  void ReceiveRelroInfo(int fd) {
    // Receive relro information from parent.
    int relro_fd = -1;
    if (ReceiveFd(fd, &relro_fd) < 0) {
      Panic("Could not receive %s relro descriptor from parent", this->name);
    }

    printf("Child received %s relro fd %d\n", this->name, relro_fd);

    int ret = TEMP_FAILURE_RETRY(::read(fd, &this->relro, sizeof(this->relro)));
    if (ret != static_cast<int>(sizeof(this->relro))) {
      Panic("Could not receive %s relro information from parent", this->name);
    }

    this->relro.fd = relro_fd;
    printf("Child received %s relro start=%p size=%p\n",
           this->name,
           (void*)this->relro.start,
           (void*)this->relro.size);
  }

  void UseSharedRelro(crazy_context_t* context) {
    if (!crazy_library_use_shared_relro(this->library,
                                        context,
                                        this->relro.start,
                                        this->relro.size,
                                        this->relro.fd)) {
      Panic("Could not use %s shared RELRO: %s\n",
            this->name,
            crazy_context_get_error(context));
    }
  }
};

}  // namespace

#endif  // TEST_UTIL_H
