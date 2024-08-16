// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/disk_cache/blockfile/sparse_control.h"

#include <stdint.h>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/base/interval.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/entry_impl.h"
#include "net/disk_cache/blockfile/file.h"
#include "net/disk_cache/net_log_parameters.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

using base::Time;

namespace {

// Stream of the sparse data index.
const int kSparseIndex = 2;

// Stream of the sparse data.
const int kSparseData = 1;

// We can have up to 64k children.
const int kMaxMapSize = 8 * 1024;

// The maximum number of bytes that a child can store.
const int kMaxEntrySize = 0x100000;

// How much we can address. 8 KiB bitmap (kMaxMapSize above) gives us offsets
// up to 64 GiB.
const int64_t kMaxEndOffset = 8ll * kMaxMapSize * kMaxEntrySize;

// The size of each data block (tracked by the child allocation bitmap).
const int kBlockSize = 1024;

// Returns the name of a child entry given the base_name and signature of the
// parent and the child_id.
// If the entry is called entry_name, child entries will be named something
// like Range_entry_name:XXX:YYY where XXX is the entry signature and YYY is the
// number of the particular child.
std::string GenerateChildName(const std::string& base_name,
                              int64_t signature,
                              int64_t child_id) {
  return base::StringPrintf("Range_%s:%" PRIx64 ":%" PRIx64, base_name.c_str(),
                            signature, child_id);
}

// This class deletes the children of a sparse entry.
class ChildrenDeleter
    : public base::RefCounted<ChildrenDeleter>,
      public disk_cache::FileIOCallback {
 public:
  ChildrenDeleter(disk_cache::BackendImpl* backend, const std::string& name)
      : backend_(backend->GetWeakPtr()), name_(name) {}

  ChildrenDeleter(const ChildrenDeleter&) = delete;
  ChildrenDeleter& operator=(const ChildrenDeleter&) = delete;

  void OnFileIOComplete(int bytes_copied) override;

  // Two ways of deleting the children: if we have the children map, use Start()
  // directly, otherwise pass the data address to ReadData().
  void Start(std::unique_ptr<char[]> buffer, int len);
  void ReadData(disk_cache::Addr address, int len);

 private:
  friend class base::RefCounted<ChildrenDeleter>;
  ~ChildrenDeleter() override = default;

  void DeleteChildren();

  base::WeakPtr<disk_cache::BackendImpl> backend_;
  std::string name_;
  disk_cache::Bitmap children_map_;
  int64_t signature_ = 0;
  std::unique_ptr<char[]> buffer_;
};

// This is the callback of the file operation.
void ChildrenDeleter::OnFileIOComplete(int bytes_copied) {
  Start(std::move(buffer_), bytes_copied);
}

void ChildrenDeleter::Start(std::unique_ptr<char[]> buffer, int len) {
  buffer_ = std::move(buffer);
  if (len < static_cast<int>(sizeof(disk_cache::SparseData)))
    return Release();

  // Just copy the information from |buffer|, delete |buffer| and start deleting
  // the child entries.
  disk_cache::SparseData* data =
      reinterpret_cast<disk_cache::SparseData*>(buffer_.get());
  signature_ = data->header.signature;

  int num_bits = (len - sizeof(disk_cache::SparseHeader)) * 8;
  children_map_.Resize(num_bits, false);
  children_map_.SetMap(data->bitmap, num_bits / 32);
  buffer_.reset();

  DeleteChildren();
}

void ChildrenDeleter::ReadData(disk_cache::Addr address, int len) {
  DCHECK(address.is_block_file());
  if (!backend_.get())
    return Release();

  disk_cache::File* file(backend_->File(address));
  if (!file)
    return Release();

  size_t file_offset = address.start_block() * address.BlockSize() +
                       disk_cache::kBlockHeaderSize;

  buffer_ = std::make_unique<char[]>(len);
  bool completed;
  if (!file->Read(buffer_.get(), len, file_offset, this, &completed))
    return Release();

  if (completed)
    OnFileIOComplete(len);

  // And wait until OnFileIOComplete gets called.
}

void ChildrenDeleter::DeleteChildren() {
  int child_id = 0;
  if (!children_map_.FindNextSetBit(&child_id) || !backend_.get()) {
    // We are done. Just delete this object.
    return Release();
  }
  std::string child_name = GenerateChildName(name_, signature_, child_id);
  backend_->SyncDoomEntry(child_name);
  children_map_.Set(child_id, false);

  // Post a task to delete the next child.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ChildrenDeleter::DeleteChildren, this));
}

// Returns the NetLog event type corresponding to a SparseOperation.
net::NetLogEventType GetSparseEventType(
    disk_cache::SparseControl::SparseOperation operation) {
  switch (operation) {
    case disk_cache::SparseControl::kReadOperation:
      return net::NetLogEventType::SPARSE_READ;
    case disk_cache::SparseControl::kWriteOperation:
      return net::NetLogEventType::SPARSE_WRITE;
    case disk_cache::SparseControl::kGetRangeOperation:
      return net::NetLogEventType::SPARSE_GET_RANGE;
    default:
      NOTREACHED_IN_MIGRATION();
      return net::NetLogEventType::CANCELLED;
  }
}

// Logs the end event for |operation| on a child entry.  Range operations log
// no events for each child they search through.
void LogChildOperationEnd(const net::NetLogWithSource& net_log,
                          disk_cache::SparseControl::SparseOperation operation,
                          int result) {
  if (net_log.IsCapturing()) {
    net::NetLogEventType event_type;
    switch (operation) {
      case disk_cache::SparseControl::kReadOperation:
        event_type = net::NetLogEventType::SPARSE_READ_CHILD_DATA;
        break;
      case disk_cache::SparseControl::kWriteOperation:
        event_type = net::NetLogEventType::SPARSE_WRITE_CHILD_DATA;
        break;
      case disk_cache::SparseControl::kGetRangeOperation:
        return;
      default:
        NOTREACHED_IN_MIGRATION();
        return;
    }
    net_log.EndEventWithNetErrorCode(event_type, result);
  }
}

}  // namespace.

namespace disk_cache {

SparseControl::SparseControl(EntryImpl* entry)
    : entry_(entry),
      child_map_(child_data_.bitmap, kNumSparseBits, kNumSparseBits / 32) {
  memset(&sparse_header_, 0, sizeof(sparse_header_));
  memset(&child_data_, 0, sizeof(child_data_));
}

SparseControl::~SparseControl() {
  if (child_)
    CloseChild();
  if (init_)
    WriteSparseData();
}

int SparseControl::Init() {
  DCHECK(!init_);

  // We should not have sparse data for the exposed entry.
  if (entry_->GetDataSize(kSparseData))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  // Now see if there is something where we store our data.
  int rv = net::OK;
  int data_len = entry_->GetDataSize(kSparseIndex);
  if (!data_len) {
    rv = CreateSparseEntry();
  } else {
    rv = OpenSparseEntry(data_len);
  }

  if (rv == net::OK)
    init_ = true;
  return rv;
}

bool SparseControl::CouldBeSparse() const {
  DCHECK(!init_);

  if (entry_->GetDataSize(kSparseData))
    return false;

  // We don't verify the data, just see if it could be there.
  return (entry_->GetDataSize(kSparseIndex) != 0);
}

int SparseControl::StartIO(SparseOperation op,
                           int64_t offset,
                           net::IOBuffer* buf,
                           int buf_len,
                           CompletionOnceCallback callback) {
  DCHECK(init_);
  // We don't support simultaneous IO for sparse data.
  if (operation_ != kNoOperation)
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  int64_t end_offset = 0;  // non-inclusive.
  if (!base::CheckAdd(offset, buf_len).AssignIfValid(&end_offset)) {
    // Writes aren't permitted to try to cross the end of address space;
    // read/GetAvailableRange clip.
    if (op == kWriteOperation)
      return net::ERR_INVALID_ARGUMENT;
    else
      end_offset = std::numeric_limits<int64_t>::max();
  }

  if (offset >= kMaxEndOffset) {
    // Interval is within valid offset space, but completely outside backend
    // supported range. Permit GetAvailableRange to say "nothing here", actual
    // I/O fails.
    if (op == kGetRangeOperation)
      return 0;
    else
      return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
  }

  if (end_offset > kMaxEndOffset) {
    // Interval is partially what the backend can handle. Fail writes, clip
    // reads.
    if (op == kWriteOperation)
      return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
    else
      end_offset = kMaxEndOffset;
  }

  DCHECK_GE(end_offset, offset);
  buf_len = end_offset - offset;

  DCHECK(!user_buf_.get());
  DCHECK(user_callback_.is_null());

  if (!buf && (op == kReadOperation || op == kWriteOperation))
    return 0;

  // Copy the operation parameters.
  operation_ = op;
  offset_ = offset;
  user_buf_ = buf ? base::MakeRefCounted<net::DrainableIOBuffer>(buf, buf_len)
                  : nullptr;
  buf_len_ = buf_len;
  user_callback_ = std::move(callback);

  result_ = 0;
  pending_ = false;
  finished_ = false;
  abort_ = false;

  if (entry_->net_log().IsCapturing()) {
    NetLogSparseOperation(entry_->net_log(), GetSparseEventType(operation_),
                          net::NetLogEventPhase::BEGIN, offset_, buf_len_);
  }
  DoChildrenIO();

  if (!pending_) {
    // Everything was done synchronously.
    operation_ = kNoOperation;
    user_buf_ = nullptr;
    user_callback_.Reset();
    return result_;
  }

  return net::ERR_IO_PENDING;
}

RangeResult SparseControl::GetAvailableRange(int64_t offset, int len) {
  DCHECK(init_);
  // We don't support simultaneous IO for sparse data.
  if (operation_ != kNoOperation)
    return RangeResult(net::ERR_CACHE_OPERATION_NOT_SUPPORTED);

  range_found_ = false;
  int result = StartIO(kGetRangeOperation, offset, nullptr, len,
                       CompletionOnceCallback());
  if (range_found_)
    return RangeResult(offset_, result);

  // This is a failure. We want to return a valid start value if it's just an
  // empty range, though.
  if (result < 0)
    return RangeResult(static_cast<net::Error>(result));
  return RangeResult(offset, 0);
}

void SparseControl::CancelIO() {
  if (operation_ == kNoOperation)
    return;
  abort_ = true;
}

int SparseControl::ReadyToUse(CompletionOnceCallback callback) {
  if (!abort_)
    return net::OK;

  // We'll grab another reference to keep this object alive because we just have
  // one extra reference due to the pending IO operation itself, but we'll
  // release that one before invoking user_callback_.
  entry_->AddRef();  // Balanced in DoAbortCallbacks.
  abort_callbacks_.push_back(std::move(callback));
  return net::ERR_IO_PENDING;
}

// Static
void SparseControl::DeleteChildren(EntryImpl* entry) {
  DCHECK(entry->GetEntryFlags() & PARENT_ENTRY);
  int data_len = entry->GetDataSize(kSparseIndex);
  if (data_len < static_cast<int>(sizeof(SparseData)) ||
      entry->GetDataSize(kSparseData))
    return;

  int map_len = data_len - sizeof(SparseHeader);
  if (map_len > kMaxMapSize || map_len % 4)
    return;

  std::unique_ptr<char[]> buffer;
  Addr address;
  entry->GetData(kSparseIndex, &buffer, &address);
  if (!buffer && !address.is_initialized())
    return;

  entry->net_log().AddEvent(net::NetLogEventType::SPARSE_DELETE_CHILDREN);

  DCHECK(entry->backend_.get());
  ChildrenDeleter* deleter = new ChildrenDeleter(entry->backend_.get(),
                                                 entry->GetKey());
  // The object will self destruct when finished.
  deleter->AddRef();

  if (buffer) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ChildrenDeleter::Start, deleter,
                                  std::move(buffer), data_len));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChildrenDeleter::ReadData, deleter, address, data_len));
  }
}

// We are going to start using this entry to store sparse data, so we have to
// initialize our control info.
int SparseControl::CreateSparseEntry() {
  if (CHILD_ENTRY & entry_->GetEntryFlags())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  memset(&sparse_header_, 0, sizeof(sparse_header_));
  sparse_header_.signature = Time::Now().ToInternalValue();
  sparse_header_.magic = kIndexMagic;
  sparse_header_.parent_key_len = entry_->GetKey().size();
  children_map_.Resize(kNumSparseBits, true);

  // Save the header. The bitmap is saved in the destructor.
  scoped_refptr<net::IOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
      base::as_chars(base::span_from_ref(sparse_header_)));

  int rv = entry_->WriteData(kSparseIndex, 0, buf.get(), sizeof(sparse_header_),
                             CompletionOnceCallback(), false);
  if (rv != sizeof(sparse_header_)) {
    DLOG(ERROR) << "Unable to save sparse_header_";
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
  }

  entry_->SetEntryFlags(PARENT_ENTRY);
  return net::OK;
}

// We are opening an entry from disk. Make sure that our control data is there.
int SparseControl::OpenSparseEntry(int data_len) {
  if (data_len < static_cast<int>(sizeof(SparseData)))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (entry_->GetDataSize(kSparseData))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  if (!(PARENT_ENTRY & entry_->GetEntryFlags()))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  // Don't go over board with the bitmap.
  int map_len = data_len - sizeof(sparse_header_);
  if (map_len > kMaxMapSize || map_len % 4)
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  scoped_refptr<net::IOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
      base::as_chars(base::span_from_ref(sparse_header_)));

  // Read header.
  int rv = entry_->ReadData(kSparseIndex, 0, buf.get(), sizeof(sparse_header_),
                            CompletionOnceCallback());
  if (rv != static_cast<int>(sizeof(sparse_header_)))
    return net::ERR_CACHE_READ_FAILURE;

  // The real validation should be performed by the caller. This is just to
  // double check.
  if (sparse_header_.magic != kIndexMagic ||
      sparse_header_.parent_key_len !=
          static_cast<int>(entry_->GetKey().size()))
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  // Read the actual bitmap.
  buf = base::MakeRefCounted<net::IOBufferWithSize>(map_len);
  rv = entry_->ReadData(kSparseIndex, sizeof(sparse_header_), buf.get(),
                        map_len, CompletionOnceCallback());
  if (rv != map_len)
    return net::ERR_CACHE_READ_FAILURE;

  // Grow the bitmap to the current size and copy the bits.
  children_map_.Resize(map_len * 8, false);
  children_map_.SetMap(reinterpret_cast<uint32_t*>(buf->data()), map_len);
  return net::OK;
}

bool SparseControl::OpenChild() {
  DCHECK_GE(result_, 0);

  std::string key = GenerateChildKey();
  if (child_) {
    // Keep using the same child or open another one?.
    if (key == child_->GetKey())
      return true;
    CloseChild();
  }

  // See if we are tracking this child.
  if (!ChildPresent())
    return ContinueWithoutChild(key);

  if (!entry_->backend_.get())
    return false;

  child_ = entry_->backend_->OpenEntryImpl(key);
  if (!child_)
    return ContinueWithoutChild(key);

  if (!(CHILD_ENTRY & child_->GetEntryFlags()) ||
      child_->GetDataSize(kSparseIndex) < static_cast<int>(sizeof(child_data_)))
    return KillChildAndContinue(key, false);

  auto buf = base::MakeRefCounted<net::WrappedIOBuffer>(
      base::as_chars(base::span_from_ref(child_data_)));

  // Read signature.
  int rv = child_->ReadData(kSparseIndex, 0, buf.get(), sizeof(child_data_),
                            CompletionOnceCallback());
  if (rv != sizeof(child_data_))
    return KillChildAndContinue(key, true);  // This is a fatal failure.

  if (child_data_.header.signature != sparse_header_.signature ||
      child_data_.header.magic != kIndexMagic)
    return KillChildAndContinue(key, false);

  if (child_data_.header.last_block_len < 0 ||
      child_data_.header.last_block_len >= kBlockSize) {
    // Make sure these values are always within range.
    child_data_.header.last_block_len = 0;
    child_data_.header.last_block = -1;
  }

  return true;
}

void SparseControl::CloseChild() {
  auto buf = base::MakeRefCounted<net::WrappedIOBuffer>(
      base::as_chars(base::span_from_ref(child_data_)));

  // Save the allocation bitmap before closing the child entry.
  int rv = child_->WriteData(kSparseIndex, 0, buf.get(), sizeof(child_data_),
                             CompletionOnceCallback(), false);
  if (rv != sizeof(child_data_))
    DLOG(ERROR) << "Failed to save child data";
  child_ = nullptr;
}

std::string SparseControl::GenerateChildKey() {
  return GenerateChildName(entry_->GetKey(), sparse_header_.signature,
                           offset_ >> 20);
}

// We are deleting the child because something went wrong.
bool SparseControl::KillChildAndContinue(const std::string& key, bool fatal) {
  SetChildBit(false);
  child_->DoomImpl();
  child_ = nullptr;
  if (fatal) {
    result_ = net::ERR_CACHE_READ_FAILURE;
    return false;
  }
  return ContinueWithoutChild(key);
}

// We were not able to open this child; see what we can do.
bool SparseControl::ContinueWithoutChild(const std::string& key) {
  if (kReadOperation == operation_)
    return false;
  if (kGetRangeOperation == operation_)
    return true;

  if (!entry_->backend_.get())
    return false;

  child_ = entry_->backend_->CreateEntryImpl(key);
  if (!child_) {
    child_ = nullptr;
    result_ = net::ERR_CACHE_READ_FAILURE;
    return false;
  }
  // Write signature.
  InitChildData();
  return true;
}

bool SparseControl::ChildPresent() {
  int child_bit = static_cast<int>(offset_ >> 20);
  if (children_map_.Size() <= child_bit)
    return false;

  return children_map_.Get(child_bit);
}

void SparseControl::SetChildBit(bool value) {
  int child_bit = static_cast<int>(offset_ >> 20);

  // We may have to increase the bitmap of child entries.
  if (children_map_.Size() <= child_bit)
    children_map_.Resize(Bitmap::RequiredArraySize(child_bit + 1) * 32, true);

  children_map_.Set(child_bit, value);
}

void SparseControl::WriteSparseData() {
  auto buf = base::MakeRefCounted<net::WrappedIOBuffer>(
      base::as_chars(children_map_.GetSpan()));

  int rv = entry_->WriteData(kSparseIndex, sizeof(sparse_header_), buf.get(),
                             buf->size(), CompletionOnceCallback(), false);
  if (rv != buf->size()) {
    DLOG(ERROR) << "Unable to save sparse map";
  }
}

bool SparseControl::VerifyRange() {
  DCHECK_GE(result_, 0);

  child_offset_ = static_cast<int>(offset_) & (kMaxEntrySize - 1);
  child_len_ = std::min(buf_len_, kMaxEntrySize - child_offset_);

  // We can write to (or get info from) anywhere in this child.
  if (operation_ != kReadOperation)
    return true;

  // Check that there are no holes in this range.
  int last_bit = (child_offset_ + child_len_ + 1023) >> 10;
  int start = child_offset_ >> 10;
  if (child_map_.FindNextBit(&start, last_bit, false)) {
    // Something is not here.
    DCHECK_GE(child_data_.header.last_block_len, 0);
    DCHECK_LT(child_data_.header.last_block_len, kBlockSize);
    int partial_block_len = PartialBlockLength(start);
    if (start == child_offset_ >> 10) {
      // It looks like we don't have anything.
      if (partial_block_len <= (child_offset_ & (kBlockSize - 1)))
        return false;
    }

    // We have the first part.
    child_len_ = (start << 10) - child_offset_;
    if (partial_block_len) {
      // We may have a few extra bytes.
      child_len_ = std::min(child_len_ + partial_block_len, buf_len_);
    }
    // There is no need to read more after this one.
    buf_len_ = child_len_;
  }
  return true;
}

void SparseControl::UpdateRange(int result) {
  if (result <= 0 || operation_ != kWriteOperation)
    return;

  DCHECK_GE(child_data_.header.last_block_len, 0);
  DCHECK_LT(child_data_.header.last_block_len, kBlockSize);

  // Write the bitmap.
  int first_bit = child_offset_ >> 10;
  int block_offset = child_offset_ & (kBlockSize - 1);
  if (block_offset && (child_data_.header.last_block != first_bit ||
                       child_data_.header.last_block_len < block_offset)) {
    // The first block is not completely filled; ignore it.
    first_bit++;
  }

  int last_bit = (child_offset_ + result) >> 10;
  block_offset = (child_offset_ + result) & (kBlockSize - 1);

  // This condition will hit with the following criteria:
  // 1. The first byte doesn't follow the last write.
  // 2. The first byte is in the middle of a block.
  // 3. The first byte and the last byte are in the same block.
  if (first_bit > last_bit)
    return;

  if (block_offset && !child_map_.Get(last_bit)) {
    // The last block is not completely filled; save it for later.
    child_data_.header.last_block = last_bit;
    child_data_.header.last_block_len = block_offset;
  } else {
    child_data_.header.last_block = -1;
  }

  child_map_.SetRange(first_bit, last_bit, true);
}

int SparseControl::PartialBlockLength(int block_index) const {
  if (block_index == child_data_.header.last_block)
    return child_data_.header.last_block_len;

  // This is really empty.
  return 0;
}

void SparseControl::InitChildData() {
  child_->SetEntryFlags(CHILD_ENTRY);

  memset(&child_data_, 0, sizeof(child_data_));
  child_data_.header = sparse_header_;

  auto buf = base::MakeRefCounted<net::WrappedIOBuffer>(
      base::as_chars(base::span_from_ref(child_data_)));

  int rv = child_->WriteData(kSparseIndex, 0, buf.get(), sizeof(child_data_),
                             CompletionOnceCallback(), false);
  if (rv != sizeof(child_data_))
    DLOG(ERROR) << "Failed to save child data";
  SetChildBit(true);
}

void SparseControl::DoChildrenIO() {
  while (DoChildIO()) continue;

  // Range operations are finished synchronously, often without setting
  // |finished_| to true.
  if (kGetRangeOperation == operation_ && entry_->net_log().IsCapturing()) {
    entry_->net_log().EndEvent(net::NetLogEventType::SPARSE_GET_RANGE, [&] {
      return CreateNetLogGetAvailableRangeResultParams(
          RangeResult(offset_, result_));
    });
  }
  if (finished_) {
    if (kGetRangeOperation != operation_ && entry_->net_log().IsCapturing()) {
      entry_->net_log().EndEvent(GetSparseEventType(operation_));
    }
    if (pending_)
      DoUserCallback();  // Don't touch this object after this point.
  }
}

bool SparseControl::DoChildIO() {
  finished_ = true;
  if (!buf_len_ || result_ < 0)
    return false;

  if (!OpenChild())
    return false;

  if (!VerifyRange())
    return false;

  // We have more work to do. Let's not trigger a callback to the caller.
  finished_ = false;
  CompletionOnceCallback callback;
  if (!user_callback_.is_null()) {
    callback = base::BindOnce(&SparseControl::OnChildIOCompleted,
                              base::Unretained(this));
  }

  int rv = 0;
  switch (operation_) {
    case kReadOperation:
      if (entry_->net_log().IsCapturing()) {
        NetLogSparseReadWrite(entry_->net_log(),
                              net::NetLogEventType::SPARSE_READ_CHILD_DATA,
                              net::NetLogEventPhase::BEGIN,
                              child_->net_log().source(), child_len_);
      }
      rv = child_->ReadDataImpl(kSparseData, child_offset_, user_buf_.get(),
                                child_len_, std::move(callback));
      break;
    case kWriteOperation:
      if (entry_->net_log().IsCapturing()) {
        NetLogSparseReadWrite(entry_->net_log(),
                              net::NetLogEventType::SPARSE_WRITE_CHILD_DATA,
                              net::NetLogEventPhase::BEGIN,
                              child_->net_log().source(), child_len_);
      }
      rv = child_->WriteDataImpl(kSparseData, child_offset_, user_buf_.get(),
                                 child_len_, std::move(callback), false);
      break;
    case kGetRangeOperation:
      rv = DoGetAvailableRange();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (rv == net::ERR_IO_PENDING) {
    if (!pending_) {
      pending_ = true;
      // The child will protect himself against closing the entry while IO is in
      // progress. However, this entry can still be closed, and that would not
      // be a good thing for us, so we increase the refcount until we're
      // finished doing sparse stuff.
      entry_->AddRef();  // Balanced in DoUserCallback.
    }
    return false;
  }
  if (!rv)
    return false;

  DoChildIOCompleted(rv);
  return true;
}

int SparseControl::DoGetAvailableRange() {
  if (!child_)
    return child_len_;  // Move on to the next child.

  // Blockfile splits sparse files into multiple child entries, each responsible
  // for managing 1MiB of address space. This method is responsible for
  // implementing GetAvailableRange within a single child.
  //
  // Input:
  //   |child_offset_|, |child_len_|:
  //     describe range in current child's address space the client requested.
  //   |offset_| is equivalent to |child_offset_| but in global address space.
  //
  //   For example if this were child [2] and the original call was for
  //   [0x200005, 0x200007) then |offset_| would be 0x200005, |child_offset_|
  //   would be 5, and |child_len| would be 2.
  //
  // Output:
  //   If nothing found:
  //     return |child_len_|
  //
  //   If something found:
  //     |result_| gets the length of the available range.
  //     |offset_| gets the global address of beginning of the available range.
  //     |range_found_| get true to signal SparseControl::GetAvailableRange().
  //     return 0 to exit loop.
  net::Interval<int> to_find(child_offset_, child_offset_ + child_len_);

  // Within each child, valid portions are mostly tracked via the |child_map_|
  // bitmap which marks which 1KiB 'blocks' have valid data. Scan the bitmap
  // for the first contiguous range of set bits that's relevant to the range
  // [child_offset_, child_offset_ + len)
  int first_bit = child_offset_ >> 10;
  int last_bit = (child_offset_ + child_len_ + kBlockSize - 1) >> 10;
  int found = first_bit;
  int bits_found = child_map_.FindBits(&found, last_bit, true);
  net::Interval<int> bitmap_range(found * kBlockSize,
                                  found * kBlockSize + bits_found * kBlockSize);

  // Bits on the bitmap should only be set when the corresponding block was
  // fully written (it's really being used). If a block is partially used, it
  // has to start with valid data, the length of the valid data is saved in
  // |header.last_block_len| and the block number saved in |header.last_block|.
  // This is updated after every write; with |header.last_block| set to -1
  // if no sub-KiB range is being tracked.
  net::Interval<int> last_write_range;
  if (child_data_.header.last_block >= 0) {
    last_write_range =
        net::Interval<int>(child_data_.header.last_block * kBlockSize,
                           child_data_.header.last_block * kBlockSize +
                               child_data_.header.last_block_len);
  }

  // Often |last_write_range| is contiguously after |bitmap_range|, but not
  // always. See if they can be combined.
  if (!last_write_range.Empty() && !bitmap_range.Empty() &&
      bitmap_range.max() == last_write_range.min()) {
    bitmap_range.SetMax(last_write_range.max());
    last_write_range.Clear();
  }

  // Do any of them have anything relevant?
  bitmap_range.IntersectWith(to_find);
  last_write_range.IntersectWith(to_find);

  // Now return the earliest non-empty interval, if any.
  net::Interval<int> result_range = bitmap_range;
  if (bitmap_range.Empty() || (!last_write_range.Empty() &&
                               last_write_range.min() < bitmap_range.min()))
    result_range = last_write_range;

  if (result_range.Empty()) {
    // Nothing found, so we just skip over this child.
    return child_len_;
  }

  // Package up our results.
  range_found_ = true;
  offset_ += result_range.min() - child_offset_;
  result_ = result_range.max() - result_range.min();
  return 0;
}

void SparseControl::DoChildIOCompleted(int result) {
  LogChildOperationEnd(entry_->net_log(), operation_, result);
  if (result < 0) {
    // We fail the whole operation if we encounter an error.
    result_ = result;
    return;
  }

  UpdateRange(result);

  result_ += result;
  offset_ += result;
  buf_len_ -= result;

  // We'll be reusing the user provided buffer for the next chunk.
  if (buf_len_ && user_buf_.get())
    user_buf_->DidConsume(result);
}

void SparseControl::OnChildIOCompleted(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  DoChildIOCompleted(result);

  if (abort_) {
    // We'll return the current result of the operation, which may be less than
    // the bytes to read or write, but the user cancelled the operation.
    abort_ = false;
    if (entry_->net_log().IsCapturing()) {
      entry_->net_log().AddEvent(net::NetLogEventType::CANCELLED);
      entry_->net_log().EndEvent(GetSparseEventType(operation_));
    }
    // We have an indirect reference to this object for every callback so if
    // there is only one callback, we may delete this object before reaching
    // DoAbortCallbacks.
    bool has_abort_callbacks = !abort_callbacks_.empty();
    DoUserCallback();
    if (has_abort_callbacks)
      DoAbortCallbacks();
    return;
  }

  // We are running a callback from the message loop. It's time to restart what
  // we were doing before.
  DoChildrenIO();
}

void SparseControl::DoUserCallback() {
  DCHECK(!user_callback_.is_null());
  CompletionOnceCallback cb = std::move(user_callback_);
  user_buf_ = nullptr;
  pending_ = false;
  operation_ = kNoOperation;
  int rv = result_;
  entry_->Release();  // Don't touch object after this line.
  std::move(cb).Run(rv);
}

void SparseControl::DoAbortCallbacks() {
  std::vector<CompletionOnceCallback> abort_callbacks;
  abort_callbacks.swap(abort_callbacks_);

  for (CompletionOnceCallback& callback : abort_callbacks) {
    // Releasing all references to entry_ may result in the destruction of this
    // object so we should not be touching it after the last Release().
    entry_->Release();
    std::move(callback).Run(net::OK);
  }
}

}  // namespace disk_cache
