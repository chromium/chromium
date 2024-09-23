// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/disk_cache/blockfile/entry_impl.h"

#include <limits>
#include <memory>

#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/bitmap.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/disk_cache/blockfile/sparse_control.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/net_log_parameters.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"

using base::Time;
using base::TimeTicks;

namespace {

// Index for the file used to store the key, if any (files_[kKeyFileIndex]).
const int kKeyFileIndex = 3;

// This class implements FileIOCallback to buffer the callback from a file IO
// operation from the actual net class.
class SyncCallback: public disk_cache::FileIOCallback {
 public:
  // |end_event_type| is the event type to log on completion.  Logs nothing on
  // discard, or when the NetLog is not set to log all events.
  SyncCallback(scoped_refptr<disk_cache::EntryImpl> entry,
               net::IOBuffer* buffer,
               net::CompletionOnceCallback callback,
               net::NetLogEventType end_event_type)
      : entry_(std::move(entry)),
        callback_(std::move(callback)),
        buf_(buffer),
        end_event_type_(end_event_type) {
    entry_->IncrementIoCount();
  }

  SyncCallback(const SyncCallback&) = delete;
  SyncCallback& operator=(const SyncCallback&) = delete;

  ~SyncCallback() override = default;

  void OnFileIOComplete(int bytes_copied) override;
  void Discard();

 private:
  scoped_refptr<disk_cache::EntryImpl> entry_;
  net::CompletionOnceCallback callback_;
  scoped_refptr<net::IOBuffer> buf_;
  const net::NetLogEventType end_event_type_;
};

void SyncCallback::OnFileIOComplete(int bytes_copied) {
  entry_->DecrementIoCount();
  if (!callback_.is_null()) {
    if (entry_->net_log().IsCapturing()) {
      disk_cache::NetLogReadWriteComplete(entry_->net_log(), end_event_type_,
                                          net::NetLogEventPhase::END,
                                          bytes_copied);
    }
    buf_ = nullptr;  // Release the buffer before invoking the callback.
    std::move(callback_).Run(bytes_copied);
  }
  delete this;
}

void SyncCallback::Discard() {
  callback_.Reset();
  buf_ = nullptr;
  OnFileIOComplete(0);
}

const int kMaxBufferSize = 1024 * 1024;  // 1 MB.

}  // namespace

namespace disk_cache {

// This class handles individual memory buffers that store data before it is
// sent to disk. The buffer can start at any offset, but if we try to write to
// anywhere in the first 16KB of the file (kMaxBlockSize), we set the offset to
// zero. The buffer grows up to a size determined by the backend, to keep the
// total memory used under control.
class EntryImpl::UserBuffer {
 public:
  explicit UserBuffer(BackendImpl* backend) : backend_(backend->GetWeakPtr()) {
    buffer_.reserve(kMaxBlockSize);
  }

  UserBuffer(const UserBuffer&) = delete;
  UserBuffer& operator=(const UserBuffer&) = delete;

  ~UserBuffer() {
    if (backend_.get())
      backend_->BufferDeleted(capacity() - kMaxBlockSize);
  }

  // Returns true if we can handle writing |len| bytes to |offset|.
  bool PreWrite(int offset, int len);

  // Truncates the buffer to |offset| bytes.
  void Truncate(int offset);

  // Writes |len| bytes from |buf| at the given |offset|.
  void Write(int offset, IOBuffer* buf, int len);

  // Returns true if we can read |len| bytes from |offset|, given that the
  // actual file has |eof| bytes stored. Note that the number of bytes to read
  // may be modified by this method even though it returns false: that means we
  // should do a smaller read from disk.
  bool PreRead(int eof, int offset, int* len);

  // Read |len| bytes from |buf| at the given |offset|.
  int Read(int offset, IOBuffer* buf, int len);

  // Prepare this buffer for reuse.
  void Reset();

  char* Data() { return buffer_.data(); }
  int Size() { return static_cast<int>(buffer_.size()); }
  int Start() { return offset_; }
  int End() { return offset_ + Size(); }

 private:
  int capacity() { return static_cast<int>(buffer_.capacity()); }
  bool GrowBuffer(int required, int limit);

  base::WeakPtr<BackendImpl> backend_;
  int offset_ = 0;
  std::vector<char> buffer_;
  bool grow_allowed_ = true;
};

bool EntryImpl::UserBuffer::PreWrite(int offset, int len) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(len, 0);
  DCHECK_GE(offset + len, 0);

  // We don't want to write before our current start.
  if (offset < offset_)
    return false;

  // Lets get the common case out of the way.
  if (offset + len <= capacity())
    return true;

  // If we are writing to the first 16K (kMaxBlockSize), we want to keep the
  // buffer offset_ at 0.
  if (!Size() && offset > kMaxBlockSize)
    return GrowBuffer(len, kMaxBufferSize);

  int required = offset - offset_ + len;
  return GrowBuffer(required, kMaxBufferSize * 6 / 5);
}

void EntryImpl::UserBuffer::Truncate(int offset) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(offset, offset_);
  DVLOG(3) << "Buffer truncate at " << offset << " current " << offset_;

  offset -= offset_;
  if (Size() >= offset)
    buffer_.resize(offset);
}

void EntryImpl::UserBuffer::Write(int offset, IOBuffer* buf, int len) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(len, 0);
  DCHECK_GE(offset + len, 0);

  // 0-length writes that don't extend can just be ignored here, and are safe
  // even if they're are before offset_, as truncates are handled elsewhere.
  if (len == 0 && offset < End())
    return;

  DCHECK_GE(offset, offset_);
  DVLOG(3) << "Buffer write at " << offset << " current " << offset_;

  if (!Size() && offset > kMaxBlockSize)
    offset_ = offset;

  offset -= offset_;

  if (offset > Size())
    buffer_.resize(offset);

  if (!len)
    return;

  char* buffer = buf->data();
  int valid_len = Size() - offset;
  int copy_len = std::min(valid_len, len);
  if (copy_len) {
    memcpy(&buffer_[offset], buffer, copy_len);
    len -= copy_len;
    buffer += copy_len;
  }
  if (!len)
    return;

  buffer_.insert(buffer_.end(), buffer, buffer + len);
}

bool EntryImpl::UserBuffer::PreRead(int eof, int offset, int* len) {
  DCHECK_GE(offset, 0);
  DCHECK_GT(*len, 0);

  if (offset < offset_) {
    // We are reading before this buffer.
    if (offset >= eof)
      return true;

    // If the read overlaps with the buffer, change its length so that there is
    // no overlap.
    *len = std::min(*len, offset_ - offset);
    *len = std::min(*len, eof - offset);

    // We should read from disk.
    return false;
  }

  if (!Size())
    return false;

  // See if we can fulfill the first part of the operation.
  return (offset - offset_ < Size());
}

int EntryImpl::UserBuffer::Read(int offset, IOBuffer* buf, int len) {
  DCHECK_GE(offset, 0);
  DCHECK_GT(len, 0);
  DCHECK(Size() || offset < offset_);

  int clean_bytes = 0;
  if (offset < offset_) {
    // We don't have a file so lets fill the first part with 0.
    clean_bytes = std::min(offset_ - offset, len);
    memset(buf->data(), 0, clean_bytes);
    if (len == clean_bytes)
      return len;
    offset = offset_;
    len -= clean_bytes;
  }

  int start = offset - offset_;
  int available = Size() - start;
  DCHECK_GE(start, 0);
  DCHECK_GE(available, 0);
  len = std::min(len, available);
  memcpy(buf->data() + clean_bytes, &buffer_[start], len);
  return len + clean_bytes;
}

void EntryImpl::UserBuffer::Reset() {
  if (!grow_allowed_) {
    if (backend_.get())
      backend_->BufferDeleted(capacity() - kMaxBlockSize);
    grow_allowed_ = true;
    std::vector<char> tmp;
    buffer_.swap(tmp);
    buffer_.reserve(kMaxBlockSize);
  }
  offset_ = 0;
  buffer_.clear();
}

bool EntryImpl::UserBuffer::GrowBuffer(int required, int limit) {
  DCHECK_GE(required, 0);
  int current_size = capacity();
  if (required <= current_size)
    return true;

  if (required > limit)
    return false;

  if (!backend_.get())
    return false;

  int to_add = std::max(required - current_size, kMaxBlockSize * 4);
  to_add = std::max(current_size, to_add);
  required = std::min(current_size + to_add, limit);

  grow_allowed_ = backend_->IsAllocAllowed(current_size, required);
  if (!grow_allowed_)
    return false;

  DVLOG(3) << "Buffer grow to " << required;

  buffer_.reserve(required);
  return true;
}

// ------------------------------------------------------------------------

EntryImpl::EntryImpl(BackendImpl* backend, Addr address, bool read_only)
    : entry_(nullptr, Addr(0)),
      node_(nullptr, Addr(0)),
      backend_(backend->GetWeakPtr()),
      read_only_(read_only) {
  entry_.LazyInit(backend->File(address), address);
}

void EntryImpl::DoomImpl() {
  if (doomed_ || !backend_.get())
    return;

  SetPointerForInvalidEntry(backend_->GetCurrentEntryId());
  backend_->InternalDoomEntry(this);
}

int EntryImpl::ReadDataImpl(int index,
                            int offset,
                            IOBuffer* buf,
                            int buf_len,
                            CompletionOnceCallback callback) {
  if (net_log_.IsCapturing()) {
    NetLogReadWriteData(net_log_, net::NetLogEventType::ENTRY_READ_DATA,
                        net::NetLogEventPhase::BEGIN, index, offset, buf_len,
                        false);
  }

  int result =
      InternalReadData(index, offset, buf, buf_len, std::move(callback));

  if (result != net::ERR_IO_PENDING && net_log_.IsCapturing()) {
    NetLogReadWriteComplete(net_log_, net::NetLogEventType::ENTRY_READ_DATA,
                            net::NetLogEventPhase::END, result);
  }
  return result;
}

int EntryImpl::WriteDataImpl(int index,
                             int offset,
                             IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback,
                             bool truncate) {
  if (net_log_.IsCapturing()) {
    NetLogReadWriteData(net_log_, net::NetLogEventType::ENTRY_WRITE_DATA,
                        net::NetLogEventPhase::BEGIN, index, offset, buf_len,
                        truncate);
  }

  int result = InternalWriteData(index, offset, buf, buf_len,
                                 std::move(callback), truncate);

  if (result != net::ERR_IO_PENDING && net_log_.IsCapturing()) {
    NetLogReadWriteComplete(net_log_, net::NetLogEventType::ENTRY_WRITE_DATA,
                            net::NetLogEventPhase::END, result);
  }
  return result;
}

int EntryImpl::ReadSparseDataImpl(int64_t offset,
                                  IOBuffer* buf,
                                  int buf_len,
                                  CompletionOnceCallback callback) {
  DCHECK(node_.Data()->dirty || read_only_);
  int result = InitSparseData();
  if (net::OK != result)
    return result;

  result = sparse_->StartIO(SparseControl::kReadOperation, offset, buf, buf_len,
                            std::move(callback));
  return result;
}

int EntryImpl::WriteSparseDataImpl(int64_t offset,
                                   IOBuffer* buf,
                                   int buf_len,
                                   CompletionOnceCallback callback) {
  DCHECK(node_.Data()->dirty || read_only_);
  int result = InitSparseData();
  if (net::OK != result)
    return result;

  result = sparse_->StartIO(SparseControl::kWriteOperation, offset, buf,
                            buf_len, std::move(callback));
  return result;
}

RangeResult EntryImpl::GetAvailableRangeImpl(int64_t offset, int len) {
  int result = InitSparseData();
  if (net::OK != result)
    return RangeResult(static_cast<net::Error>(result));

  return sparse_->GetAvailableRange(offset, len);
}

void EntryImpl::CancelSparseIOImpl() {
  if (!sparse_.get())
    return;

  sparse_->CancelIO();
}

int EntryImpl::ReadyForSparseIOImpl(CompletionOnceCallback callback) {
  DCHECK(sparse_.get());
  return sparse_->ReadyToUse(std::move(callback));
}

uint32_t EntryImpl::GetHash() {
  return entry_.Data()->hash;
}

bool EntryImpl::CreateEntry(Addr node_address,
                            const std::string& key,
                            uint32_t hash) {
  EntryStore* entry_store = entry_.Data();
  RankingsNode* node = node_.Data();
  memset(entry_store, 0, sizeof(EntryStore) * entry_.address().num_blocks());
  memset(node, 0, sizeof(RankingsNode));
  if (!node_.LazyInit(backend_->File(node_address), node_address))
    return false;

  entry_store->rankings_node = node_address.value();
  node->contents = entry_.address().value();

  entry_store->hash = hash;
  entry_store->creation_time = Time::Now().ToInternalValue();
  entry_store->key_len = static_cast<int32_t>(key.size());
  if (entry_store->key_len > kMaxInternalKeyLength) {
    Addr address(0);
    if (!CreateBlock(entry_store->key_len + 1, &address))
      return false;

    entry_store->long_key = address.value();
    File* key_file = GetBackingFile(address, kKeyFileIndex);
    key_ = key;

    size_t offset = 0;
    if (address.is_block_file())
      offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

    if (!key_file || !key_file->Write(key.data(), key.size() + 1, offset)) {
      DeleteData(address, kKeyFileIndex);
      return false;
    }

    if (address.is_separate_file())
      key_file->SetLength(key.size() + 1);
  } else {
    memcpy(entry_store->key, key.data(), key.size());
    entry_store->key[key.size()] = '\0';
  }
  backend_->ModifyStorageSize(0, static_cast<int32_t>(key.size()));
  node->dirty = backend_->GetCurrentEntryId();
  return true;
}

bool EntryImpl::IsSameEntry(const std::string& key, uint32_t hash) {
  if (entry_.Data()->hash != hash ||
      static_cast<size_t>(entry_.Data()->key_len) != key.size())
    return false;

  return (key.compare(GetKey()) == 0);
}

void EntryImpl::InternalDoom() {
  net_log_.AddEvent(net::NetLogEventType::ENTRY_DOOM);
  DCHECK(node_.HasData());
  if (!node_.Data()->dirty) {
    node_.Data()->dirty = backend_->GetCurrentEntryId();
    node_.Store();
  }
  doomed_ = true;
}

void EntryImpl::DeleteEntryData(bool everything) {
  DCHECK(doomed_ || !everything);

  if (GetEntryFlags() & PARENT_ENTRY) {
    // We have some child entries that must go away.
    SparseControl::DeleteChildren(this);
  }

  for (int index = 0; index < kNumStreams; index++) {
    Addr address(entry_.Data()->data_addr[index]);
    if (address.is_initialized()) {
      backend_->ModifyStorageSize(entry_.Data()->data_size[index] -
                                      unreported_size_[index], 0);
      entry_.Data()->data_addr[index] = 0;
      entry_.Data()->data_size[index] = 0;
      entry_.Store();
      DeleteData(address, index);
    }
  }

  if (!everything)
    return;

  // Remove all traces of this entry.
  backend_->RemoveEntry(this);

  // Note that at this point node_ and entry_ are just two blocks of data, and
  // even if they reference each other, nobody should be referencing them.

  Addr address(entry_.Data()->long_key);
  DeleteData(address, kKeyFileIndex);
  backend_->ModifyStorageSize(entry_.Data()->key_len, 0);

  backend_->DeleteBlock(entry_.address(), true);
  entry_.Discard();

  if (!LeaveRankingsBehind()) {
    backend_->DeleteBlock(node_.address(), true);
    node_.Discard();
  }
}

CacheAddr EntryImpl::GetNextAddress() {
  return entry_.Data()->next;
}

void EntryImpl::SetNextAddress(Addr address) {
  DCHECK_NE(address.value(), entry_.address().value());
  entry_.Data()->next = address.value();
  bool success = entry_.Store();
  DCHECK(success);
}

bool EntryImpl::LoadNodeAddress() {
  Addr address(entry_.Data()->rankings_node);
  if (!node_.LazyInit(backend_->File(address), address))
    return false;
  return node_.Load();
}

bool EntryImpl::Update() {
  DCHECK(node_.HasData());

  if (read_only_)
    return true;

  RankingsNode* rankings = node_.Data();
  if (!rankings->dirty) {
    rankings->dirty = backend_->GetCurrentEntryId();
    if (!node_.Store())
      return false;
  }
  return true;
}

void EntryImpl::SetDirtyFlag(int32_t current_id) {
  DCHECK(node_.HasData());
  if (node_.Data()->dirty && current_id != node_.Data()->dirty)
    dirty_ = true;

  if (!current_id)
    dirty_ = true;
}

void EntryImpl::SetPointerForInvalidEntry(int32_t new_id) {
  node_.Data()->dirty = new_id;
  node_.Store();
}

bool EntryImpl::LeaveRankingsBehind() {
  return !node_.Data()->contents;
}

// This only includes checks that relate to the first block of the entry (the
// first 256 bytes), and values that should be set from the entry creation.
// Basically, even if there is something wrong with this entry, we want to see
// if it is possible to load the rankings node and delete them together.
bool EntryImpl::SanityCheck() {
  if (!entry_.VerifyHash())
    return false;

  EntryStore* stored = entry_.Data();
  if (!stored->rankings_node || stored->key_len <= 0)
    return false;

  if (stored->reuse_count < 0 || stored->refetch_count < 0)
    return false;

  Addr rankings_addr(stored->rankings_node);
  if (!rankings_addr.SanityCheckForRankings())
    return false;

  Addr next_addr(stored->next);
  if (next_addr.is_initialized() && !next_addr.SanityCheckForEntry()) {
    STRESS_NOTREACHED();
    return false;
  }
  STRESS_DCHECK(next_addr.value() != entry_.address().value());

  if (stored->state > ENTRY_DOOMED || stored->state < ENTRY_NORMAL)
    return false;

  Addr key_addr(stored->long_key);
  if ((stored->key_len <= kMaxInternalKeyLength && key_addr.is_initialized()) ||
      (stored->key_len > kMaxInternalKeyLength && !key_addr.is_initialized()))
    return false;

  if (!key_addr.SanityCheck())
    return false;

  if (key_addr.is_initialized() &&
      ((stored->key_len < kMaxBlockSize && key_addr.is_separate_file()) ||
       (stored->key_len >= kMaxBlockSize && key_addr.is_block_file())))
    return false;

  int num_blocks = NumBlocksForEntry(stored->key_len);
  if (entry_.address().num_blocks() != num_blocks)
    return false;

  return true;
}

bool EntryImpl::DataSanityCheck() {
  EntryStore* stored = entry_.Data();
  Addr key_addr(stored->long_key);

  // The key must be NULL terminated.
  if (!key_addr.is_initialized() && stored->key[stored->key_len])
    return false;

  if (stored->hash != base::PersistentHash(GetKey()))
    return false;

  for (int i = 0; i < kNumStreams; i++) {
    Addr data_addr(stored->data_addr[i]);
    int data_size = stored->data_size[i];
    if (data_size < 0)
      return false;
    if (!data_size && data_addr.is_initialized())
      return false;
    if (!data_addr.SanityCheck())
      return false;
    if (!data_size)
      continue;
    if (data_size <= kMaxBlockSize && data_addr.is_separate_file())
      return false;
    if (data_size > kMaxBlockSize && data_addr.is_block_file())
      return false;
  }
  return true;
}

void EntryImpl::FixForDelete() {
  EntryStore* stored = entry_.Data();
  Addr key_addr(stored->long_key);

  if (!key_addr.is_initialized())
    stored->key[stored->key_len] = '\0';

  for (int i = 0; i < kNumStreams; i++) {
    Addr data_addr(stored->data_addr[i]);
    int data_size = stored->data_size[i];
    if (data_addr.is_initialized()) {
      if ((data_size <= kMaxBlockSize && data_addr.is_separate_file()) ||
          (data_size > kMaxBlockSize && data_addr.is_block_file()) ||
          !data_addr.SanityCheck()) {
        STRESS_NOTREACHED();
        // The address is weird so don't attempt to delete it.
        stored->data_addr[i] = 0;
        // In general, trust the stored size as it should be in sync with the
        // total size tracked by the backend.
      }
    }
    if (data_size < 0)
      stored->data_size[i] = 0;
  }
  entry_.Store();
}

void EntryImpl::IncrementIoCount() {
  backend_->IncrementIoCount();
}

void EntryImpl::DecrementIoCount() {
  if (backend_.get())
    backend_->DecrementIoCount();
}

void EntryImpl::OnEntryCreated(BackendImpl* backend) {
  // Just grab a reference to the backround queue.
  background_queue_ = backend->GetBackgroundQueue();
}

void EntryImpl::SetTimes(base::Time last_used, base::Time last_modified) {
  node_.Data()->last_used = last_used.ToInternalValue();
  node_.Data()->last_modified = last_modified.ToInternalValue();
  node_.set_modified();
}

void EntryImpl::BeginLogging(net::NetLog* net_log, bool created) {
  DCHECK(!net_log_.net_log());
  net_log_ = net::NetLogWithSource::Make(
      net_log, net::NetLogSourceType::DISK_CACHE_ENTRY);
  net_log_.BeginEvent(net::NetLogEventType::DISK_CACHE_ENTRY_IMPL, [&] {
    return CreateNetLogParametersEntryCreationParams(this, created);
  });
}

const net::NetLogWithSource& EntryImpl::net_log() const {
  return net_log_;
}

// static
int EntryImpl::NumBlocksForEntry(int key_size) {
  // The longest key that can be stored using one block.
  int key1_len =
      static_cast<int>(sizeof(EntryStore) - offsetof(EntryStore, key));

  if (key_size < key1_len || key_size > kMaxInternalKeyLength)
    return 1;

  return ((key_size - key1_len) / 256 + 2);
}

// ------------------------------------------------------------------------

void EntryImpl::Doom() {
  if (background_queue_.get())
    background_queue_->DoomEntryImpl(this);
}

void EntryImpl::Close() {
  if (background_queue_.get())
    background_queue_->CloseEntryImpl(this);
}

std::string EntryImpl::GetKey() const {
  CacheEntryBlock* entry = const_cast<CacheEntryBlock*>(&entry_);
  int key_len = entry->Data()->key_len;
  if (key_len <= kMaxInternalKeyLength)
    return std::string(entry->Data()->key, key_len);

  // We keep a copy of the key so that we can always return it, even if the
  // backend is disabled.
  if (!key_.empty())
    return key_;

  Addr address(entry->Data()->long_key);
  DCHECK(address.is_initialized());
  size_t offset = 0;
  if (address.is_block_file())
    offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

  static_assert(kNumStreams == kKeyFileIndex, "invalid key index");
  File* key_file = const_cast<EntryImpl*>(this)->GetBackingFile(address,
                                                                kKeyFileIndex);
  if (!key_file)
    return std::string();

  // We store a trailing \0 on disk.
  if (!offset && key_file->GetLength() != static_cast<size_t>(key_len + 1)) {
    return std::string();
  }

  // Do not attempt read up to the expected on-disk '\0' --- which would be
  // |key_len + 1| bytes total --- as if due to a corrupt file it isn't |key_|
  // would get its internal nul messed up.
  key_.resize(key_len);
  if (!key_file->Read(key_.data(), key_.size(), offset)) {
    key_.clear();
  }
  DCHECK_LE(strlen(key_.data()), static_cast<size_t>(key_len));
  return key_;
}

Time EntryImpl::GetLastUsed() const {
  CacheRankingsBlock* node = const_cast<CacheRankingsBlock*>(&node_);
  return Time::FromInternalValue(node->Data()->last_used);
}

Time EntryImpl::GetLastModified() const {
  CacheRankingsBlock* node = const_cast<CacheRankingsBlock*>(&node_);
  return Time::FromInternalValue(node->Data()->last_modified);
}

int32_t EntryImpl::GetDataSize(int index) const {
  if (index < 0 || index >= kNumStreams)
    return 0;

  CacheEntryBlock* entry = const_cast<CacheEntryBlock*>(&entry_);
  return entry->Data()->data_size[index];
}

int EntryImpl::ReadData(int index,
                        int offset,
                        IOBuffer* buf,
                        int buf_len,
                        CompletionOnceCallback callback) {
  if (callback.is_null())
    return ReadDataImpl(index, offset, buf, buf_len, std::move(callback));

  DCHECK(node_.Data()->dirty || read_only_);
  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  int entry_size = entry_.Data()->data_size[index];
  if (offset >= entry_size || offset < 0 || !buf_len)
    return 0;

  if (buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  if (!background_queue_.get())
    return net::ERR_UNEXPECTED;

  background_queue_->ReadData(this, index, offset, buf, buf_len,
                              std::move(callback));
  return net::ERR_IO_PENDING;
}

int EntryImpl::WriteData(int index,
                         int offset,
                         IOBuffer* buf,
                         int buf_len,
                         CompletionOnceCallback callback,
                         bool truncate) {
  if (callback.is_null()) {
    return WriteDataImpl(index, offset, buf, buf_len, std::move(callback),
                         truncate);
  }

  DCHECK(node_.Data()->dirty || read_only_);
  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  if (!background_queue_.get())
    return net::ERR_UNEXPECTED;

  background_queue_->WriteData(this, index, offset, buf, buf_len, truncate,
                               std::move(callback));
  return net::ERR_IO_PENDING;
}

int EntryImpl::ReadSparseData(int64_t offset,
                              IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  if (callback.is_null())
    return ReadSparseDataImpl(offset, buf, buf_len, std::move(callback));

  if (!background_queue_.get())
    return net::ERR_UNEXPECTED;

  background_queue_->ReadSparseData(this, offset, buf, buf_len,
                                    std::move(callback));
  return net::ERR_IO_PENDING;
}

int EntryImpl::WriteSparseData(int64_t offset,
                               IOBuffer* buf,
                               int buf_len,
                               CompletionOnceCallback callback) {
  if (callback.is_null())
    return WriteSparseDataImpl(offset, buf, buf_len, std::move(callback));

  if (!background_queue_.get())
    return net::ERR_UNEXPECTED;

  background_queue_->WriteSparseData(this, offset, buf, buf_len,
                                     std::move(callback));
  return net::ERR_IO_PENDING;
}

RangeResult EntryImpl::GetAvailableRange(int64_t offset,
                                         int len,
                                         RangeResultCallback callback) {
  if (!background_queue_.get())
    return RangeResult(net::ERR_UNEXPECTED);

  background_queue_->GetAvailableRange(this, offset, len, std::move(callback));
  return RangeResult(net::ERR_IO_PENDING);
}

bool EntryImpl::CouldBeSparse() const {
  if (sparse_.get())
    return true;

  auto sparse = std::make_unique<SparseControl>(const_cast<EntryImpl*>(this));
  return sparse->CouldBeSparse();
}

void EntryImpl::CancelSparseIO() {
  if (background_queue_.get())
    background_queue_->CancelSparseIO(this);
}

net::Error EntryImpl::ReadyForSparseIO(CompletionOnceCallback callback) {
  if (!sparse_.get())
    return net::OK;

  if (!background_queue_.get())
    return net::ERR_UNEXPECTED;

  background_queue_->ReadyForSparseIO(this, std::move(callback));
  return net::ERR_IO_PENDING;
}

void EntryImpl::SetLastUsedTimeForTest(base::Time time) {
  SetTimes(time, time);
}

// When an entry is deleted from the cache, we clean up all the data associated
// with it for two reasons: to simplify the reuse of the block (we know that any
// unused block is filled with zeros), and to simplify the handling of write /
// read partial information from an entry (don't have to worry about returning
// data related to a previous cache entry because the range was not fully
// written before).
EntryImpl::~EntryImpl() {
  if (!backend_.get()) {
    entry_.clear_modified();
    node_.clear_modified();
    return;
  }

  // Save the sparse info to disk. This will generate IO for this entry and
  // maybe for a child entry, so it is important to do it before deleting this
  // entry.
  sparse_.reset();

  // Remove this entry from the list of open entries.
  backend_->OnEntryDestroyBegin(entry_.address());

  if (doomed_) {
    DeleteEntryData(true);
  } else {
#if defined(NET_BUILD_STRESS_CACHE)
    SanityCheck();
#endif
    net_log_.AddEvent(net::NetLogEventType::ENTRY_CLOSE);
    bool ret = true;
    for (int index = 0; index < kNumStreams; index++) {
      if (user_buffers_[index].get()) {
        ret = Flush(index, 0);
        if (!ret)
          LOG(ERROR) << "Failed to save user data";
      }
      if (unreported_size_[index]) {
        backend_->ModifyStorageSize(
            entry_.Data()->data_size[index] - unreported_size_[index],
            entry_.Data()->data_size[index]);
      }
    }

    if (!ret) {
      // There was a failure writing the actual data. Mark the entry as dirty.
      int current_id = backend_->GetCurrentEntryId();
      node_.Data()->dirty = current_id == 1 ? -1 : current_id - 1;
      node_.Store();
    } else if (node_.HasData() && !dirty_ && node_.Data()->dirty) {
      node_.Data()->dirty = 0;
      node_.Store();
    }
  }

  net_log_.EndEvent(net::NetLogEventType::DISK_CACHE_ENTRY_IMPL);
  backend_->OnEntryDestroyEnd();
}

// ------------------------------------------------------------------------

int EntryImpl::InternalReadData(int index,
                                int offset,
                                IOBuffer* buf,
                                int buf_len,
                                CompletionOnceCallback callback) {
  DCHECK(node_.Data()->dirty || read_only_);
  DVLOG(2) << "Read from " << index << " at " << offset << " : " << buf_len;
  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  int entry_size = entry_.Data()->data_size[index];
  if (offset >= entry_size || offset < 0 || !buf_len)
    return 0;

  if (buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  if (!backend_.get())
    return net::ERR_UNEXPECTED;

  int end_offset;
  if (!base::CheckAdd(offset, buf_len).AssignIfValid(&end_offset) ||
      end_offset > entry_size)
    buf_len = entry_size - offset;

  UpdateRank(false);

  backend_->OnEvent(Stats::READ_DATA);
  backend_->OnRead(buf_len);

  Addr address(entry_.Data()->data_addr[index]);
  int eof = address.is_initialized() ? entry_size : 0;
  if (user_buffers_[index].get() &&
      user_buffers_[index]->PreRead(eof, offset, &buf_len)) {
    // Complete the operation locally.
    buf_len = user_buffers_[index]->Read(offset, buf, buf_len);
    return buf_len;
  }

  address.set_value(entry_.Data()->data_addr[index]);
  if (!address.is_initialized()) {
    DoomImpl();
    return net::ERR_FAILED;
  }

  File* file = GetBackingFile(address, index);
  if (!file) {
    DoomImpl();
    LOG(ERROR) << "No file for " << std::hex << address.value();
    return net::ERR_FILE_NOT_FOUND;
  }

  size_t file_offset = offset;
  if (address.is_block_file()) {
    DCHECK_LE(offset + buf_len, kMaxBlockSize);
    file_offset += address.start_block() * address.BlockSize() +
                   kBlockHeaderSize;
  }

  SyncCallback* io_callback = nullptr;
  bool null_callback = callback.is_null();
  if (!null_callback) {
    io_callback =
        new SyncCallback(base::WrapRefCounted(this), buf, std::move(callback),
                         net::NetLogEventType::ENTRY_READ_DATA);
  }

  bool completed;
  if (!file->Read(buf->data(), buf_len, file_offset, io_callback, &completed)) {
    if (io_callback)
      io_callback->Discard();
    DoomImpl();
    return net::ERR_CACHE_READ_FAILURE;
  }

  if (io_callback && completed)
    io_callback->Discard();

  return (completed || null_callback) ? buf_len : net::ERR_IO_PENDING;
}

int EntryImpl::InternalWriteData(int index,
                                 int offset,
                                 IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback,
                                 bool truncate) {
  DCHECK(node_.Data()->dirty || read_only_);
  DVLOG(2) << "Write to " << index << " at " << offset << " : " << buf_len;
  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  if (!backend_.get())
    return net::ERR_UNEXPECTED;

  int max_file_size = backend_->MaxFileSize();

  int end_offset;
  if (offset > max_file_size || buf_len > max_file_size ||
      !base::CheckAdd(offset, buf_len).AssignIfValid(&end_offset) ||
      end_offset > max_file_size) {
    int size = base::CheckAdd(offset, buf_len)
                   .ValueOrDefault(std::numeric_limits<int32_t>::max());
    backend_->TooMuchStorageRequested(size);
    return net::ERR_FAILED;
  }

  // Read the size at this point (it may change inside prepare).
  int entry_size = entry_.Data()->data_size[index];
  bool extending = entry_size < offset + buf_len;
  truncate = truncate && entry_size > offset + buf_len;
  if (!PrepareTarget(index, offset, buf_len, truncate))
    return net::ERR_FAILED;

  if (extending || truncate)
    UpdateSize(index, entry_size, offset + buf_len);

  UpdateRank(true);

  backend_->OnEvent(Stats::WRITE_DATA);
  backend_->OnWrite(buf_len);

  if (user_buffers_[index].get()) {
    // Complete the operation locally.
    user_buffers_[index]->Write(offset, buf, buf_len);
    return buf_len;
  }

  Addr address(entry_.Data()->data_addr[index]);
  if (offset + buf_len == 0) {
    if (truncate) {
      DCHECK(!address.is_initialized());
    }
    return 0;
  }

  File* file = GetBackingFile(address, index);
  if (!file)
    return net::ERR_FILE_NOT_FOUND;

  size_t file_offset = offset;
  if (address.is_block_file()) {
    DCHECK_LE(offset + buf_len, kMaxBlockSize);
    file_offset += address.start_block() * address.BlockSize() +
                   kBlockHeaderSize;
  } else if (truncate || (extending && !buf_len)) {
    if (!file->SetLength(offset + buf_len))
      return net::ERR_FAILED;
  }

  if (!buf_len)
    return 0;

  SyncCallback* io_callback = nullptr;
  bool null_callback = callback.is_null();
  if (!null_callback) {
    io_callback = new SyncCallback(this, buf, std::move(callback),
                                   net::NetLogEventType::ENTRY_WRITE_DATA);
  }

  bool completed;
  if (!file->Write(buf->data(), buf_len, file_offset, io_callback,
                   &completed)) {
    if (io_callback)
      io_callback->Discard();
    return net::ERR_CACHE_WRITE_FAILURE;
  }

  if (io_callback && completed)
    io_callback->Discard();

  return (completed || null_callback) ? buf_len : net::ERR_IO_PENDING;
}

// ------------------------------------------------------------------------

bool EntryImpl::CreateDataBlock(int index, int size) {
  DCHECK(index >= 0 && index < kNumStreams);

  Addr address(entry_.Data()->data_addr[index]);
  if (!CreateBlock(size, &address))
    return false;

  entry_.Data()->data_addr[index] = address.value();
  entry_.Store();
  return true;
}

bool EntryImpl::CreateBlock(int size, Addr* address) {
  DCHECK(!address->is_initialized());
  if (!backend_.get())
    return false;

  FileType file_type = Addr::RequiredFileType(size);
  if (EXTERNAL == file_type) {
    if (size > backend_->MaxFileSize())
      return false;
    if (!backend_->CreateExternalFile(address))
      return false;
  } else {
    int num_blocks = Addr::RequiredBlocks(size, file_type);

    if (!backend_->CreateBlock(file_type, num_blocks, address))
      return false;
  }
  return true;
}

// Note that this method may end up modifying a block file so upon return the
// involved block will be free, and could be reused for something else. If there
// is a crash after that point (and maybe before returning to the caller), the
// entry will be left dirty... and at some point it will be discarded; it is
// important that the entry doesn't keep a reference to this address, or we'll
// end up deleting the contents of |address| once again.
void EntryImpl::DeleteData(Addr address, int index) {
  DCHECK(backend_.get());
  if (!address.is_initialized())
    return;
  if (address.is_separate_file()) {
    int failure = !base::DeleteFile(backend_->GetFileName(address));
    if (failure) {
      LOG(ERROR) << "Failed to delete " <<
          backend_->GetFileName(address).value() << " from the cache.";
    }
    if (files_[index].get())
      files_[index] = nullptr;  // Releases the object.
  } else {
    backend_->DeleteBlock(address, true);
  }
}

void EntryImpl::UpdateRank(bool modified) {
  if (!backend_.get())
    return;

  if (!doomed_) {
    // Everything is handled by the backend.
    backend_->UpdateRank(this, modified);
    return;
  }

  Time current = Time::Now();
  node_.Data()->last_used = current.ToInternalValue();

  if (modified)
    node_.Data()->last_modified = current.ToInternalValue();
}

File* EntryImpl::GetBackingFile(Addr address, int index) {
  if (!backend_.get())
    return nullptr;

  File* file;
  if (address.is_separate_file())
    file = GetExternalFile(address, index);
  else
    file = backend_->File(address);
  return file;
}

File* EntryImpl::GetExternalFile(Addr address, int index) {
  DCHECK(index >= 0 && index <= kKeyFileIndex);
  if (!files_[index].get()) {
    // For a key file, use mixed mode IO.
    auto file = base::MakeRefCounted<File>(kKeyFileIndex == index);
    if (file->Init(backend_->GetFileName(address)))
      files_[index].swap(file);
  }
  return files_[index].get();
}

// We keep a memory buffer for everything that ends up stored on a block file
// (because we don't know yet the final data size), and for some of the data
// that end up on external files. This function will initialize that memory
// buffer and / or the files needed to store the data.
//
// In general, a buffer may overlap data already stored on disk, and in that
// case, the contents of the buffer are the most accurate. It may also extend
// the file, but we don't want to read from disk just to keep the buffer up to
// date. This means that as soon as there is a chance to get confused about what
// is the most recent version of some part of a file, we'll flush the buffer and
// reuse it for the new data. Keep in mind that the normal use pattern is quite
// simple (write sequentially from the beginning), so we optimize for handling
// that case.
bool EntryImpl::PrepareTarget(int index, int offset, int buf_len,
                              bool truncate) {
  if (truncate)
    return HandleTruncation(index, offset, buf_len);

  if (!offset && !buf_len)
    return true;

  Addr address(entry_.Data()->data_addr[index]);
  if (address.is_initialized()) {
    if (address.is_block_file() && !MoveToLocalBuffer(index))
      return false;

    if (!user_buffers_[index].get() && offset < kMaxBlockSize) {
      // We are about to create a buffer for the first 16KB, make sure that we
      // preserve existing data.
      if (!CopyToLocalBuffer(index))
        return false;
    }
  }

  if (!user_buffers_[index].get())
    user_buffers_[index] = std::make_unique<UserBuffer>(backend_.get());

  return PrepareBuffer(index, offset, buf_len);
}

// We get to this function with some data already stored. If there is a
// truncation that results on data stored internally, we'll explicitly
// handle the case here.
bool EntryImpl::HandleTruncation(int index, int offset, int buf_len) {
  Addr address(entry_.Data()->data_addr[index]);

  int current_size = entry_.Data()->data_size[index];
  int new_size = offset + buf_len;

  // This is only called when actually truncating the file, not simply when
  // truncate = true is passed to WriteData(), which could be growing the file.
  DCHECK_LT(new_size, current_size);

  if (new_size == 0) {
    // This is by far the most common scenario.
    backend_->ModifyStorageSize(current_size - unreported_size_[index], 0);
    entry_.Data()->data_addr[index] = 0;
    entry_.Data()->data_size[index] = 0;
    unreported_size_[index] = 0;
    entry_.Store();
    DeleteData(address, index);

    user_buffers_[index].reset();
    return true;
  }

  // We never postpone truncating a file, if there is one, but we may postpone
  // telling the backend about the size reduction.
  if (user_buffers_[index].get()) {
    DCHECK_GE(current_size, user_buffers_[index]->Start());
    if (!address.is_initialized()) {
      // There is no overlap between the buffer and disk.
      if (new_size > user_buffers_[index]->Start()) {
        // Truncate our buffer.
        DCHECK_LT(new_size, user_buffers_[index]->End());
        user_buffers_[index]->Truncate(new_size);

        if (offset < user_buffers_[index]->Start()) {
          // Request to write before the current buffer's start, so flush it to
          // disk and re-init.
          UpdateSize(index, current_size, new_size);
          if (!Flush(index, 0))
            return false;
          return PrepareBuffer(index, offset, buf_len);
        } else {
          // Can just stick to using the memory buffer.
          return true;
        }
      }

      // Truncated to before the current buffer, so can just discard it.
      user_buffers_[index]->Reset();
      return PrepareBuffer(index, offset, buf_len);
    }

    // There is some overlap or we need to extend the file before the
    // truncation.
    if (offset > user_buffers_[index]->Start())
      user_buffers_[index]->Truncate(new_size);
    UpdateSize(index, current_size, new_size);
    if (!Flush(index, 0))
      return false;
    user_buffers_[index].reset();
  }

  // We have data somewhere, and it is not in a buffer.
  DCHECK(!user_buffers_[index].get());
  DCHECK(address.is_initialized());

  if (new_size > kMaxBlockSize)
    return true;  // Let the operation go directly to disk.

  return ImportSeparateFile(index, offset + buf_len);
}

bool EntryImpl::CopyToLocalBuffer(int index) {
  Addr address(entry_.Data()->data_addr[index]);
  DCHECK(!user_buffers_[index].get());
  DCHECK(address.is_initialized());

  int len = std::min(entry_.Data()->data_size[index], kMaxBlockSize);
  user_buffers_[index] = std::make_unique<UserBuffer>(backend_.get());
  user_buffers_[index]->Write(len, nullptr, 0);

  File* file = GetBackingFile(address, index);
  int offset = 0;

  if (address.is_block_file())
    offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

  if (!file || !file->Read(user_buffers_[index]->Data(), len, offset, nullptr,
                           nullptr)) {
    user_buffers_[index].reset();
    return false;
  }
  return true;
}

bool EntryImpl::MoveToLocalBuffer(int index) {
  if (!CopyToLocalBuffer(index))
    return false;

  Addr address(entry_.Data()->data_addr[index]);
  entry_.Data()->data_addr[index] = 0;
  entry_.Store();
  DeleteData(address, index);

  // If we lose this entry we'll see it as zero sized.
  int len = entry_.Data()->data_size[index];
  backend_->ModifyStorageSize(len - unreported_size_[index], 0);
  unreported_size_[index] = len;
  return true;
}

bool EntryImpl::ImportSeparateFile(int index, int new_size) {
  if (entry_.Data()->data_size[index] > new_size)
    UpdateSize(index, entry_.Data()->data_size[index], new_size);

  return MoveToLocalBuffer(index);
}

bool EntryImpl::PrepareBuffer(int index, int offset, int buf_len) {
  DCHECK(user_buffers_[index].get());
  if ((user_buffers_[index]->End() && offset > user_buffers_[index]->End()) ||
      offset > entry_.Data()->data_size[index]) {
    // We are about to extend the buffer or the file (with zeros), so make sure
    // that we are not overwriting anything.
    Addr address(entry_.Data()->data_addr[index]);
    if (address.is_initialized() && address.is_separate_file()) {
      if (!Flush(index, 0))
        return false;
      // There is an actual file already, and we don't want to keep track of
      // its length so we let this operation go straight to disk.
      // The only case when a buffer is allowed to extend the file (as in fill
      // with zeros before the start) is when there is no file yet to extend.
      user_buffers_[index].reset();
      return true;
    }
  }

  if (!user_buffers_[index]->PreWrite(offset, buf_len)) {
    if (!Flush(index, offset + buf_len))
      return false;

    // Lets try again.
    if (offset > user_buffers_[index]->End() ||
        !user_buffers_[index]->PreWrite(offset, buf_len)) {
      // We cannot complete the operation with a buffer.
      DCHECK(!user_buffers_[index]->Size());
      DCHECK(!user_buffers_[index]->Start());
      user_buffers_[index].reset();
    }
  }
  return true;
}

bool EntryImpl::Flush(int index, int min_len) {
  Addr address(entry_.Data()->data_addr[index]);
  DCHECK(user_buffers_[index].get());
  DCHECK(!address.is_initialized() || address.is_separate_file());
  DVLOG(3) << "Flush";

  int size = std::max(entry_.Data()->data_size[index], min_len);
  if (size && !address.is_initialized() && !CreateDataBlock(index, size))
    return false;

  if (!entry_.Data()->data_size[index]) {
    DCHECK(!user_buffers_[index]->Size());
    return true;
  }

  address.set_value(entry_.Data()->data_addr[index]);

  int len = user_buffers_[index]->Size();
  int offset = user_buffers_[index]->Start();
  if (!len && !offset)
    return true;

  if (address.is_block_file()) {
    DCHECK_EQ(len, entry_.Data()->data_size[index]);
    DCHECK(!offset);
    offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;
  }

  File* file = GetBackingFile(address, index);
  if (!file)
    return false;

  if (!file->Write(user_buffers_[index]->Data(), len, offset, nullptr, nullptr))
    return false;
  user_buffers_[index]->Reset();

  return true;
}

void EntryImpl::UpdateSize(int index, int old_size, int new_size) {
  if (entry_.Data()->data_size[index] == new_size)
    return;

  unreported_size_[index] += new_size - old_size;
  entry_.Data()->data_size[index] = new_size;
  entry_.set_modified();
}

int EntryImpl::InitSparseData() {
  if (sparse_.get())
    return net::OK;

  // Use a local variable so that sparse_ never goes from 'valid' to NULL.
  auto sparse = std::make_unique<SparseControl>(this);
  int result = sparse->Init();
  if (net::OK == result)
    sparse_.swap(sparse);

  return result;
}

void EntryImpl::SetEntryFlags(uint32_t flags) {
  entry_.Data()->flags |= flags;
  entry_.set_modified();
}

uint32_t EntryImpl::GetEntryFlags() {
  return entry_.Data()->flags;
}

void EntryImpl::GetData(int index,
                        std::unique_ptr<char[]>* buffer,
                        Addr* address) {
  DCHECK(backend_.get());
  if (user_buffers_[index].get() && user_buffers_[index]->Size() &&
      !user_buffers_[index]->Start()) {
    // The data is already in memory, just copy it and we're done.
    int data_len = entry_.Data()->data_size[index];
    if (data_len <= user_buffers_[index]->Size()) {
      DCHECK(!user_buffers_[index]->Start());
      *buffer = std::make_unique<char[]>(data_len);
      memcpy(buffer->get(), user_buffers_[index]->Data(), data_len);
      return;
    }
  }

  // Bad news: we'd have to read the info from disk so instead we'll just tell
  // the caller where to read from.
  *buffer = nullptr;
  address->set_value(entry_.Data()->data_addr[index]);
  if (address->is_initialized()) {
    // Prevent us from deleting the block from the backing store.
    backend_->ModifyStorageSize(entry_.Data()->data_size[index] -
                                    unreported_size_[index], 0);
    entry_.Data()->data_addr[index] = 0;
    entry_.Data()->data_size[index] = 0;
  }
}

}  // namespace disk_cache
