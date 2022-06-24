// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <cstring>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"

namespace gpu::webgpu {

namespace {
// TODO(dawn:549) Tune this timeout once we have some more data on what it
// should be.
static constexpr base::TimeDelta kCacheOpTimeout = base::Milliseconds(50);
}  // namespace

// Custom deleter used for entries to close them on the internal backend thread.
struct OnTaskRunnerEntryDeleter {
  explicit OnTaskRunnerEntryDeleter(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(task_runner) {}
  ~OnTaskRunnerEntryDeleter() = default;

  OnTaskRunnerEntryDeleter(OnTaskRunnerEntryDeleter&&) = default;
  OnTaskRunnerEntryDeleter& operator=(OnTaskRunnerEntryDeleter&&) = default;

  void operator()(disk_cache::Entry* ptr) {
    if (ptr) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&disk_cache::Entry::Close, base::Unretained(ptr)));
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

using ScopedEntryPtr =
    std::unique_ptr<disk_cache::Entry, OnTaskRunnerEntryDeleter>;

class DawnCacheOperation
    : public base::RefCountedThreadSafe<DawnCacheOperation> {
 public:
  enum Type {
    kRead,
    kWrite,
  };

  DawnCacheOperation(scoped_refptr<base::SingleThreadTaskRunner> backend_thread,
                     Type type,
                     size_t buffer_size);

  // Runs this operation with the given key on the given backend.
  void Run(const std::string& key, disk_cache::Backend* backend);

  // Waits for the completion of this operation up to the timeout. Returns true
  // iff the operation completed before the timeout.
  bool TimedWait(base::TimeDelta timeout) { return signal_.TimedWait(timeout); }

  // Returns the result of the operation. If the operation is not complete, this
  // always returns 0.
  size_t Result() const { return result_size_; }
  void* Data() { return buffer_->data(); }

 private:
  friend class base::RefCountedThreadSafe<DawnCacheOperation>;
  ~DawnCacheOperation() = default;

  // Returns the entry size or clamps errors (negative values) to 0.
  size_t GetEntrySize() const;

  // Callback to handle when an entry has been opened.
  void OnOpenedEntry(disk_cache::EntryResult entry);

  // Callback to handle when an operation has completed with a status. Note that
  // statuses in the backend API are ints. Non-negative values indicate the size
  // of the result while negative results are used to indicate some sort of
  // error. For negative values, this will be a call to OnOpCompleteSize with 0
  // as the argument.
  void OnOpCompleteStatus(int status);

  // Callback to handle a valid entry size (or 0 when the operation did not
  // complete successfully) and save it to the result of this operation.
  void OnOpCompleteSize(size_t size);

  scoped_refptr<base::SingleThreadTaskRunner> backend_thread_;
  Type type_;
  scoped_refptr<net::IOBuffer> buffer_;
  size_t buffer_size_;
  ScopedEntryPtr entry_;
  size_t result_size_ = 0;
  base::WaitableEvent signal_;
};

DawnCacheOperation::DawnCacheOperation(
    scoped_refptr<base::SingleThreadTaskRunner> backend_thread,
    Type type,
    size_t buffer_size)
    : backend_thread_(backend_thread),
      type_(type),
      buffer_(buffer_size > 0
                  ? base::WrapRefCounted(new net::IOBuffer(buffer_size))
                  : nullptr),
      buffer_size_(buffer_size),
      entry_(nullptr, OnTaskRunnerEntryDeleter(backend_thread_)),
      signal_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

void DawnCacheOperation::Run(const std::string& key,
                             disk_cache::Backend* backend) {
  disk_cache::EntryResult entry = backend->OpenOrCreateEntry(
      key, net::RequestPriority::DEFAULT_PRIORITY,
      base::BindOnce(&DawnCacheOperation::OnOpenedEntry, this));
  if (entry.net_error() == net::Error::OK) {
    backend_thread_->PostTask(
        FROM_HERE, base::BindOnce(&DawnCacheOperation::OnOpenedEntry, this,
                                  std::move(entry)));
  }
}

size_t DawnCacheOperation::GetEntrySize() const {
  if (entry_ != nullptr) {
    int tmp = entry_->GetDataSize(0);
    if (tmp > 0) {
      return size_t(tmp);
    }
  }
  return 0u;
}

void DawnCacheOperation::OnOpenedEntry(disk_cache::EntryResult entry) {
  // Check if we are pending since this function may be scheduled twice if the
  // return value was asynchronous.
  if (entry.net_error() == net::ERR_IO_PENDING) {
    return;
  }

  // Backend cache implementation uses integers as both a status and a return
  // value. Non-negative values represent successful operation and doubles as
  // the size read/written depending on the operation.
  int status = 0;
  entry_.reset(entry.ReleaseEntry());
  if (entry_ == nullptr) {
    OnOpCompleteStatus(status);
    return;
  }

  switch (type_) {
    case Type::kRead: {
      if (buffer_size_ == 0) {
        // Size-read case.
        OnOpCompleteSize(GetEntrySize());
        return;
      } else {
        if (buffer_size_ != GetEntrySize()) {
          // If we are reading the actual data, the buffer size must equal the
          // entry size, otherwise we would not be able to copy the data out
          // anyways so we can just skip and return 0 to indicate that the read
          // did no occur.
          OnOpCompleteSize(0u);
          return;
        }
        buffer_ = base::WrapRefCounted(new net::IOBuffer(buffer_size_));
        status = entry_->ReadData(
            0, 0, buffer_.get(), buffer_size_,
            base::BindOnce(&DawnCacheOperation::OnOpCompleteStatus, this));
      }
      break;
    }
    case Type::kWrite: {
      status = entry_->WriteData(
          0, 0, buffer_.get(), buffer_size_,
          base::BindOnce(&DawnCacheOperation::OnOpCompleteStatus, this), false);
      break;
    }
  }

  // Both ReadData and StoreData will call OnOpCompleteStatus as a callback if
  // the returned 'status' is ERR_IO_PENDING. Otherwise, the calls will not call
  // their callback so we need to call it here, passing 'status' as the
  // argument.
  if (status != net::ERR_IO_PENDING) {
    OnOpCompleteStatus(status);
  }
}

void DawnCacheOperation::OnOpCompleteSize(size_t size) {
  result_size_ = size;
  switch (type_) {
    case Type::kRead: {
      // No-op since we already set the result.
      break;
    }
    case Type::kWrite: {
      // If the write wasn't complete (didn't write the full buffer out), we
      // mark the entry as doomed for deletion. In general, this shouldn't
      // happen, but may occur in shutdown or error scenarios.
      if (entry_ != nullptr && result_size_ != buffer_size_) {
        entry_->Doom();
      }
      break;
    }
  }
  signal_.Signal();
}

void DawnCacheOperation::OnOpCompleteStatus(int status) {
  // Any errors are negative, and we mask the status here to 0 size to indicate
  // read/write failed.
  OnOpCompleteSize(status > 0 ? size_t(status) : 0u);
}

DawnCachingInterface::DawnCachingInterface(net::CacheType cache_type,
                                           int64_t cache_size,
                                           const base::FilePath& path)
    : DawnCachingInterface(
          base::BindOnce(&DawnCachingInterface::DefaultCacheBackendFactory,
                         cache_type,
                         cache_size,
                         std::move(path))) {}

DawnCachingInterface::DawnCachingInterface(CacheBackendFactory factory)
    : factory_(std::move(factory)),
      backend_thread_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      backend_(nullptr, base::OnTaskRunnerDeleter(backend_thread_)) {}

net::Error DawnCachingInterface::Init() {
  // Initialization blocks until either a valid backend is returned or an error
  // occurs.
  std::unique_ptr<disk_cache::Backend> backend;
  base::WaitableEvent signal;
  net::Error error;
  backend_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(factory_), base::Unretained(&backend),
                     base::Unretained(&signal), base::Unretained(&error)));
  signal.Wait();
  backend_.reset(backend.release());
  return error;
}

// static
std::unique_ptr<DawnCachingInterface> DawnCachingInterface::CreateForTesting(
    CacheBackendFactory factory) {
  return std::unique_ptr<DawnCachingInterface>(
      new DawnCachingInterface(std::move(factory)));
}

DawnCachingInterface::~DawnCachingInterface() = default;

size_t DawnCachingInterface::LoadData(const void* key,
                                      size_t key_size,
                                      void* value_out,
                                      size_t value_size) {
  if (backend_ == nullptr) {
    return 0u;
  }
  std::string key_str(static_cast<const char*>(key), key_size);

  // Check if the call is a size/existence check.
  bool size_only = value_size == 0 && value_out == nullptr;

  auto op = base::WrapRefCounted(new DawnCacheOperation(
      backend_thread_, DawnCacheOperation::Type::kRead, value_size));

  backend_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&DawnCacheOperation::Run, op, std::move(key_str),
                     base::Unretained(backend_.get())));

  // For loading data, to appear synchronous, we block, but for a max timeout.
  if (!op->TimedWait(kCacheOpTimeout)) {
    return 0u;
  }

  // Copy the read memory into the actual destination buffer if we did not time
  // out reading the entry.
  if (!size_only) {
    std::memcpy(value_out, op->Data(), value_size);
  }
  return op->Result();
}

void DawnCachingInterface::StoreData(const void* key,
                                     size_t key_size,
                                     const void* value,
                                     size_t value_size) {
  if (backend_ == nullptr || value == nullptr || value_size <= 0) {
    return;
  }
  std::string key_str(static_cast<const char*>(key), key_size);

  auto op = base::WrapRefCounted(new DawnCacheOperation(
      backend_thread_, DawnCacheOperation::Type::kWrite, value_size));

  // Copy the contents of buffer into the internal buffer that will live for the
  // duration of the async operation.
  std::memcpy(op->Data(), value, value_size);

  backend_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&DawnCacheOperation::Run, op, std::move(key_str),
                     base::Unretained(backend_.get())));
}

namespace {

void BackendInitCallback(base::WaitableEvent* signal,
                         net::Error* error,
                         std::unique_ptr<disk_cache::Backend>* backend,
                         disk_cache::BackendResult result) {
  *error = result.net_error;
  *backend = std::move(result.backend);
  signal->Signal();
}

}  // namespace

// static
void DawnCachingInterface::DefaultCacheBackendFactory(
    net::CacheType cache_type,
    int64_t cache_size,
    const base::FilePath& path,
    std::unique_ptr<disk_cache::Backend>* backend,
    base::WaitableEvent* signal,
    net::Error* error) {
  disk_cache::BackendResult result = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_BLOCKFILE,
      /*file_operations=*/nullptr, path,
      /*max_bytes=*/cache_size, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr,
      base::BindOnce(&BackendInitCallback, base::Unretained(signal),
                     base::Unretained(error), base::Unretained(backend)));

  *error = result.net_error;
  if (*error == net::ERR_IO_PENDING) {
    return;
  }
  *backend = std::move(result.backend);
  signal->Signal();
}

}  // namespace gpu::webgpu
