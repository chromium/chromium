// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_system_mock.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "crazy_linker_util.h"
#include "crazy_linker_system.h"

// Unit-testing support code. This should never be compiled into
// the production code.

namespace {

using crazy::String;
using crazy::Vector;

void Panic(const char* msg, ...) {
  va_list args;
  fprintf(stderr, "PANIC: ");
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  fprintf(stderr, "\n");
  exit(1);
}

// Models a simple list of pointers to objects, which are owned by the
// list itself.
template <class T>
class List {
 public:
  List() : entries_() {}

  ~List() { Reset(); }

  void Reset() {
    for (size_t n = 0; n < entries_.GetCount(); ++n) {
      T* entry = entries_[n];
      delete entry;
      entries_[n] = NULL;
    }
    entries_.Resize(0);
  }

  // Add an item to the list, transfer ownership to it.
  void PushBack(T* item) { entries_.PushBack(item); }

  size_t GetCount() const { return entries_.GetCount(); }

  T* operator[](size_t index) { return entries_[index]; }

 private:
  crazy::Vector<T*> entries_;
};

// Models a single file entry in a mock file system.
class MockFileEntry {
 public:
  MockFileEntry() : path_(), data_() {}

  ~MockFileEntry() {}

  const char* GetPath() const { return path_.c_str(); }
  const char* GetData() const { return data_.c_str(); }
  size_t GetDataSize() const { return data_.size(); }

  void SetPath(const char* path) { path_.Assign(path); }

  void SetData(const char* data, size_t data_size) {
    data_.Assign(data, data_size);
  }

 private:
  crazy::String path_;
  crazy::String data_;
};

// Models a single mock environment variable value.
class MockEnvEntry {
 public:
  MockEnvEntry(const char* var_name, const char* var_value)
      : var_name_(var_name), var_value_(var_value) {}

  const String& GetName() const { return var_name_; }
  const String& GetValue() const { return var_value_; }

 private:
  crazy::String var_name_;
  crazy::String var_value_;
};

class MockFileHandle {
 public:
  MockFileHandle(MockFileEntry* entry) : entry_(entry), offset_(0) {}
  ~MockFileHandle() {}

  bool IsEof() const { return offset_ >= entry_->GetDataSize(); }

  bool GetString(char* buffer, size_t buffer_size) {
    const char* data = entry_->GetData();
    size_t data_size = entry_->GetDataSize();

    if (offset_ >= data_size || buffer_size == 0)
      return false;

    while (buffer_size > 1) {
      char ch = data[offset_++];
      *buffer++ = ch;
      buffer_size--;
      if (ch == '\n')
        break;
    }
    *buffer = '\0';
    return true;
  }

  ssize_t Read(void* buffer, size_t buffer_size) {
    if (buffer_size == 0)
      return 0;

    const char* data = entry_->GetData();
    size_t data_size = entry_->GetDataSize();

    size_t avail = data_size - offset_;
    if (avail == 0)
      return 0;

    if (buffer_size > avail)
      buffer_size = avail;

    ::memcpy(buffer, data + offset_, buffer_size);
    offset_ += buffer_size;

    return static_cast<int>(buffer_size);
  }

  off_t SeekTo(off_t offset) {
    if (offset < 0) {
      errno = EINVAL;
      return -1;
    }

    const char* data = entry_->GetData();
    size_t data_size = entry_->GetDataSize();

    if (offset > static_cast<off_t>(data_size)) {
      errno = EINVAL;
      return -1;
    }

    offset_ = static_cast<size_t>(offset);
    return 0;
  }

  void* Map(void* address, size_t length, int prot, int flags, off_t offset) {
    const char* data = entry_->GetData();
    size_t data_size = entry_->GetDataSize();
    if (offset_ >= data_size) {
      errno = EINVAL;
      return nullptr;
    }

    // Allocate an anonymous memory mapping, then copy the file contents
    // into it.
    void* map =
        mmap(address, length, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) {
      return nullptr;
    }

    size_t avail = data_size - offset_;
    if (avail > length)
      avail = length;

    ::memcpy(map, data + offset_, avail);

    // Restore desired protection after the write.
    mprotect(map, length, prot);

    // Done.
    return map;
  }

  int64_t GetFileSize() const {
    return static_cast<int64_t>(entry_->GetDataSize());
  }

 private:
  MockFileEntry* entry_;
  size_t offset_;
};

// Convenience class for the table of all active file descriptors in the
// mock system.
class MockFileHandleTable {
 public:
  // Constructor.
  MockFileHandleTable() = default;

  // Destructor.
  ~MockFileHandleTable() {
    for (size_t n = 0; n < handles_.GetCount(); ++n) {
      delete handles_[n];
    }
  }

  // Find the MockFileHandle corresponding to |fd|, or nullptr if this
  // is an unknown value.
  MockFileHandle* Find(int fd) const {
    if (fd < 0 || static_cast<size_t>(fd) >= handles_.GetCount()) {
      return nullptr;
    }
    return handles_[fd];
  }

  // Allocate a new file descriptor value associated with a MockFileHandle
  // instance. This takes ownership of the instance.
  int AllocateFd(MockFileHandle* handle) {
    size_t n = 0;
    for (; n < handles_.GetCount(); ++n) {
      if (!handles_[n]) {
        // Found a free descriptor in the table.
        handles_[n] = handle;
        return static_cast<int>(n);
      }
    }
    // Allocate new descriptor value.
    handles_.PushBack(handle);
    return static_cast<int>(n);
  }

  // Deallocate a file descriptor by value.
  bool DeallocateFd(int fd) {
    if (!Find(fd)) {
      return false;
    }
    delete handles_[fd];
    handles_[fd] = nullptr;
    return true;
  }

 private:
  Vector<MockFileHandle*> handles_;
};

class MockSystem {
 public:
  MockSystem() : files_(), environment_() {}

  ~MockSystem() { Reset(); }

  void SetCurrentDir(const char* path) { current_dir_ = path; }

  String GetCurrentDir() const { return current_dir_; }

  void AddFileEntry(MockFileEntry* entry) { files_.PushBack(entry); }

  void AddEnvEntry(MockEnvEntry* entry) { environment_.PushBack(entry); }

  MockFileEntry* FindFileEntry(const char* path) {
    for (size_t n = 0; n < files_.GetCount(); ++n) {
      MockFileEntry* entry = files_[n];
      if (entry->GetPath() && !strcmp(path, entry->GetPath()))
        return entry;
    }
    return NULL;
  }

  MockEnvEntry* FindEnvEntry(const char* var_name) {
    for (size_t n = 0; n < environment_.GetCount(); ++n) {
      MockEnvEntry* entry = environment_[n];
      if (!strcmp(entry->GetName().c_str(), var_name))
        return entry;
    }
    return NULL;
  }

  int OpenFd(const char* path, crazy::FileOpenMode open_mode) {
    // TODO(digit): Add write support.
    if (open_mode != crazy::FILE_OPEN_READ_ONLY)
      Panic("Unsupported open mode (%d): %s", open_mode, path);

    MockFileEntry* entry = FindFileEntry(path);
    if (!entry) {
      errno = ENOENT;
      return -1;
    }

    return handles_.AllocateFd(new MockFileHandle(entry));
  }

  MockFileHandle* FindFileHandle(int fd) const { return handles_.Find(fd); }

  void CloseFd(int fd) {
    if (!handles_.DeallocateFd(fd)) {
      Panic("Closing invalid file descriptor %d", fd);
    }
  }

  void Reset() {
    files_.Reset();
    environment_.Reset();
    current_dir_ = "/";
  }

  void Check() {
    if (!active_)
      Panic("No mock file system setup!");
  }

  void Activate() {
    if (active_)
      Panic("Double mock file system activation!");

    active_ = true;
  }

  void Deactivate() {
    if (!active_)
      Panic("Double mock file system deactivation!");

    active_ = false;
  }

 private:
  List<MockFileEntry> files_;
  List<MockEnvEntry> environment_;
  MockFileHandleTable handles_;
  String current_dir_;
  bool active_;
};

MockSystem s_mock_fs;

}  // namespace

namespace crazy {

#ifdef UNIT_TEST

bool PathExists(const char* path) {
  s_mock_fs.Check();
  return s_mock_fs.FindFileEntry(path) != NULL;
}

bool PathIsFile(const char* path) {
  // TODO(digit): Change this when support for mock directories is added.
  return PathExists(path);
}

String GetCurrentDirectory() {
  s_mock_fs.Check();
  return s_mock_fs.GetCurrentDir();
}

const char* GetEnv(const char* var_name) {
  s_mock_fs.Check();
  MockEnvEntry* entry = s_mock_fs.FindEnvEntry(var_name);
  if (!entry)
    return NULL;
  else
    return entry->GetValue().c_str();
}

ssize_t FileDescriptor::Read(void* buffer, size_t buffer_size) {
  s_mock_fs.Check();
  MockFileHandle* handle = s_mock_fs.FindFileHandle(fd_);
  if (!handle) {
    errno = EBADF;
    return -1;
  }
  return handle->Read(buffer, buffer_size);
}

off_t FileDescriptor::SeekTo(off_t offset) {
  s_mock_fs.Check();
  MockFileHandle* handle = s_mock_fs.FindFileHandle(fd_);
  if (!handle) {
    errno = EBADF;
    return -1;
  }
  return handle->SeekTo(offset);
}

void* FileDescriptor::Map(void* address,
                          size_t length,
                          int prot,
                          int flags,
                          off_t offset) {
  s_mock_fs.Check();
  MockFileHandle* handle = s_mock_fs.FindFileHandle(fd_);
  if (!handle) {
    errno = EBADF;
    return nullptr;
  }
  if ((offset & 4095) != 0) {
    errno = EINVAL;
    return nullptr;
  }
  return handle->Map(address, length, prot, flags, offset);
}

int64_t FileDescriptor::GetFileSize() const {
  s_mock_fs.Check();
  MockFileHandle* handle = s_mock_fs.FindFileHandle(fd_);
  if (!handle) {
    errno = EBADF;
    return -1;
  }
  return handle->GetFileSize();
}

// static
int FileDescriptor::DoOpenReadOnly(const char* path) {
  s_mock_fs.Check();
  return s_mock_fs.OpenFd(path, FILE_OPEN_READ_ONLY);
}

// static
int FileDescriptor::DoOpenReadWrite(const char* path) {
  s_mock_fs.Check();
  return s_mock_fs.OpenFd(path, FILE_OPEN_READ_WRITE);
}

// static
void FileDescriptor::DoClose(int fd) {
  s_mock_fs.Check();
  s_mock_fs.CloseFd(fd);
}

SystemMock::SystemMock() { s_mock_fs.Activate(); }

SystemMock::~SystemMock() {
  s_mock_fs.Deactivate();
  s_mock_fs.Reset();
}

void SystemMock::AddRegularFile(const char* path,
                                const char* data,
                                size_t data_size) {
  s_mock_fs.Check();

  MockFileEntry* entry = new MockFileEntry();
  entry->SetPath(path);
  entry->SetData(data, data_size);

  s_mock_fs.AddFileEntry(entry);
}

void SystemMock::AddEnvVariable(const char* var_name, const char* var_value) {
  s_mock_fs.Check();

  MockEnvEntry* env = new MockEnvEntry(var_name, var_value);
  s_mock_fs.AddEnvEntry(env);
}

void SystemMock::SetCurrentDir(const char* path) {
  s_mock_fs.Check();
  s_mock_fs.SetCurrentDir(path);
}

#endif  // UNIT_TEST

}  // namespace crazy
