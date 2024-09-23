// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/file.h"

#include <limits.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"

namespace {

class CompletionHandler;
// Structure used for asynchronous operations.
struct MyOverlapped {
  MyOverlapped(disk_cache::File* file, size_t offset,
               disk_cache::FileIOCallback* callback);
  ~MyOverlapped() {}
  OVERLAPPED* overlapped() {
    return &context_.overlapped;
  }

  base::MessagePumpForIO::IOContext context_;
  scoped_refptr<disk_cache::File> file_;
  scoped_refptr<CompletionHandler> completion_handler_;
  raw_ptr<disk_cache::FileIOCallback> callback_;
};

static_assert(offsetof(MyOverlapped, context_) == 0,
              "should start with overlapped");

// Helper class to handle the IO completion notifications from the message loop.
class CompletionHandler final : public base::MessagePumpForIO::IOHandler,
                                public base::RefCounted<CompletionHandler> {
 public:
  CompletionHandler() : base::MessagePumpForIO::IOHandler(FROM_HERE) {}
  static CompletionHandler* Get();

  CompletionHandler(const CompletionHandler&) = delete;
  CompletionHandler& operator=(const CompletionHandler&) = delete;

 private:
  friend class base::RefCounted<CompletionHandler>;
  ~CompletionHandler() override {}

  // implement base::MessagePumpForIO::IOHandler.
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD actual_bytes,
                     DWORD error) override;
};

CompletionHandler* CompletionHandler::Get() {
  static base::NoDestructor<scoped_refptr<CompletionHandler>> handler(
      base::MakeRefCounted<CompletionHandler>());
  return handler->get();
}

void CompletionHandler::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD actual_bytes,
    DWORD error) {
  MyOverlapped* data = reinterpret_cast<MyOverlapped*>(context);

  if (error) {
    DCHECK(!actual_bytes);
    actual_bytes = static_cast<DWORD>(net::ERR_CACHE_READ_FAILURE);
  }

  // `callback_` may self delete while in `OnFileIOComplete`.
  if (data->callback_)
    data->callback_.ExtractAsDangling()->OnFileIOComplete(
        static_cast<int>(actual_bytes));

  delete data;
}

MyOverlapped::MyOverlapped(disk_cache::File* file, size_t offset,
                           disk_cache::FileIOCallback* callback) {
  context_.overlapped.Offset = static_cast<DWORD>(offset);
  file_ = file;
  callback_ = callback;
  completion_handler_ = CompletionHandler::Get();
}

}  // namespace

namespace disk_cache {

File::File(base::File file)
    : init_(true), mixed_(true), sync_base_file_(std::move(file)) {}

bool File::Init(const base::FilePath& name) {
  DCHECK(!init_);
  if (init_)
    return false;

  DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  DWORD access = GENERIC_READ | GENERIC_WRITE | DELETE;
  base_file_ =
      base::File(CreateFile(name.value().c_str(), access, sharing, nullptr,
                            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));

  if (!base_file_.IsValid())
    return false;

  base::CurrentIOThread::Get()->RegisterIOHandler(base_file_.GetPlatformFile(),
                                                  CompletionHandler::Get());

  init_ = true;
  sync_base_file_ = base::File(CreateFile(name.value().c_str(), access, sharing,
                                          nullptr, OPEN_EXISTING, 0, nullptr));

  if (!sync_base_file_.IsValid())
    return false;

  return true;
}

bool File::IsValid() const {
  if (!init_)
    return false;
  return base_file_.IsValid() || sync_base_file_.IsValid();
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > LONG_MAX)
    return false;

  int ret = UNSAFE_TODO(
      sync_base_file_.Read(offset, static_cast<char*>(buffer), buffer_len));
  return static_cast<int>(buffer_len) == ret;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  int ret = UNSAFE_TODO(sync_base_file_.Write(
      offset, static_cast<const char*>(buffer), buffer_len));
  return static_cast<int>(buffer_len) == ret;
}

// We have to increase the ref counter of the file before performing the IO to
// prevent the completion to happen with an invalid handle (if the file is
// closed while the IO is in flight).
bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (!callback) {
    if (completed)
      *completed = true;
    return Read(buffer, buffer_len, offset);
  }

  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  MyOverlapped* data = new MyOverlapped(this, offset, callback);
  DWORD size = static_cast<DWORD>(buffer_len);

  DWORD actual;
  if (!ReadFile(base_file_.GetPlatformFile(), buffer, size, &actual,
                data->overlapped())) {
    *completed = false;
    if (GetLastError() == ERROR_IO_PENDING)
      return true;
    delete data;
    return false;
  }

  // The operation completed already. We'll be called back anyway.
  *completed = (actual == size);
  DCHECK_EQ(size, actual);
  data->callback_ = nullptr;
  data->file_ = nullptr;  // There is no reason to hold on to this anymore.
  return *completed;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset,
                 FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (!callback) {
    if (completed)
      *completed = true;
    return Write(buffer, buffer_len, offset);
  }

  return AsyncWrite(buffer, buffer_len, offset, callback, completed);
}

File::~File() = default;

base::PlatformFile File::platform_file() const {
  DCHECK(init_);
  return base_file_.IsValid() ? base_file_.GetPlatformFile() :
                                sync_base_file_.GetPlatformFile();
}

bool File::AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                      FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  DCHECK(callback);
  DCHECK(completed);
  if (buffer_len > ULONG_MAX || offset > ULONG_MAX)
    return false;

  MyOverlapped* data = new MyOverlapped(this, offset, callback);
  DWORD size = static_cast<DWORD>(buffer_len);

  DWORD actual;
  if (!WriteFile(base_file_.GetPlatformFile(), buffer, size, &actual,
                 data->overlapped())) {
    *completed = false;
    if (GetLastError() == ERROR_IO_PENDING)
      return true;
    delete data;
    return false;
  }

  // The operation completed already. We'll be called back anyway.
  *completed = (actual == size);
  DCHECK_EQ(size, actual);
  data->callback_ = nullptr;
  data->file_ = nullptr;  // There is no reason to hold on to this anymore.
  return *completed;
}

bool File::SetLength(size_t length) {
  DCHECK(init_);
  if (length > ULONG_MAX)
    return false;

  DWORD size = static_cast<DWORD>(length);
  HANDLE file = platform_file();
  if (INVALID_SET_FILE_POINTER ==
      SetFilePointer(file, size, nullptr, FILE_BEGIN))
    return false;

  return TRUE == SetEndOfFile(file);
}

size_t File::GetLength() {
  DCHECK(init_);
  LARGE_INTEGER size;
  HANDLE file = platform_file();
  if (!GetFileSizeEx(file, &size))
    return 0;
  if (size.HighPart)
    return ULONG_MAX;

  return static_cast<size_t>(size.LowPart);
}

// Static.
void File::WaitForPendingIOForTesting(int* num_pending_io) {
  // Spin on the burn-down count until the file IO completes.
  constexpr base::TimeDelta kMillisecond = base::Milliseconds(1);
  for (; *num_pending_io; base::PlatformThread::Sleep(kMillisecond)) {
    // This waits for callbacks running on worker threads.
    base::ThreadPoolInstance::Get()->FlushForTesting();  // IN-TEST
    // This waits for the "Reply" tasks running on the current MessageLoop.
    base::RunLoop().RunUntilIdle();
  }
}

// Static.
void File::DropPendingIO() {
}

}  // namespace disk_cache
