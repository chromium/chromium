// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/memory/mem_entry_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/interval.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/net_log_parameters.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"

using base::Time;

namespace disk_cache {

namespace {

constexpr int kSparseData = 1;

// Maximum size of a child of sparse entry is 2 to the power of this number.
constexpr size_t kMaxChildEntryBits = 12;

// Sparse entry children have maximum size of 4KB.
constexpr size_t kMaxChildEntrySize = 1 << kMaxChildEntryBits;

// Convert global offset to child index.
uint64_t ToChildIndex(uint64_t offset) {
  return offset >> kMaxChildEntryBits;
}

// Convert global offset to offset in child entry.
size_t ToChildOffset(uint64_t offset) {
  return static_cast<size_t>(offset & (kMaxChildEntrySize - 1));
}

// Returns a name for a child entry given the base_name of the parent and the
// child_id.  This name is only used for logging purposes.
// If the entry is called entry_name, child entries will be named something
// like Range_entry_name:YYY where YYY is the number of the particular child.
std::string GenerateChildName(const std::string& base_name, int64_t child_id) {
  return base::StringPrintf("Range_%s:%" PRId64, base_name.c_str(), child_id);
}

// Returns NetLog parameters for the creation of a MemEntryImpl. A separate
// function is needed because child entries don't store their key().
base::Value::Dict NetLogEntryCreationParams(const MemEntryImpl* entry) {
  base::Value::Dict dict;
  std::string key;
  switch (entry->type()) {
    case MemEntryImpl::EntryType::kParent:
      key = entry->key();
      break;
    case MemEntryImpl::EntryType::kChild:
      key = GenerateChildName(entry->parent()->key(), entry->child_id());
      break;
  }
  dict.Set("key", key);
  dict.Set("created", true);
  return dict;
}

}  // namespace

MemEntryImpl::MemEntryImpl(base::WeakPtr<MemBackendImpl> backend,
                           const std::string& key,
                           net::NetLog* net_log)
    : MemEntryImpl(backend,
                   key,
                   0,        // child_id
                   nullptr,  // parent
                   net_log) {
  Open();
  // Just creating the entry (without any data) could cause the storage to
  // grow beyond capacity, but we allow such infractions.
  backend_->ModifyStorageSize(GetStorageSize());
}

MemEntryImpl::MemEntryImpl(base::WeakPtr<MemBackendImpl> backend,
                           int64_t child_id,
                           MemEntryImpl* parent,
                           net::NetLog* net_log)
    : MemEntryImpl(backend,
                   std::string(),  // key
                   child_id,
                   parent,
                   net_log) {
  (*parent_->children_)[child_id] = this;
}

void MemEntryImpl::Open() {
  // Only a parent entry can be opened.
  DCHECK_EQ(EntryType::kParent, type());
  CHECK_NE(ref_count_, std::numeric_limits<uint32_t>::max());
  ++ref_count_;
  DCHECK(!doomed_);
}

bool MemEntryImpl::InUse() const {
  if (type() == EntryType::kChild)
    return parent_->InUse();

  return ref_count_ > 0;
}

int MemEntryImpl::GetStorageSize() const {
  int storage_size = static_cast<int32_t>(key_.size());
  for (const auto& i : data_)
    storage_size += i.size();
  return storage_size;
}

void MemEntryImpl::UpdateStateOnUse() {
  if (!doomed_ && backend_)
    backend_->OnEntryUpdated(this);

  last_used_ = MemBackendImpl::Now(backend_);
}

void MemEntryImpl::Doom() {
  if (!doomed_) {
    doomed_ = true;
    if (backend_)
      backend_->OnEntryDoomed(this);
    net_log_.AddEvent(net::NetLogEventType::ENTRY_DOOM);
  }
  if (!ref_count_)
    delete this;
}

void MemEntryImpl::Close() {
  DCHECK_EQ(EntryType::kParent, type());
  CHECK_GT(ref_count_, 0u);
  --ref_count_;
  if (ref_count_ == 0 && !doomed_) {
    // At this point the user is clearly done writing, so make sure there isn't
    // wastage due to exponential growth of vector for main data stream.
    Compact();
    if (children_) {
      for (const auto& child_info : *children_) {
        if (child_info.second != this)
          child_info.second->Compact();
      }
    }
  }
  if (!ref_count_ && doomed_)
    delete this;
}

std::string MemEntryImpl::GetKey() const {
  // A child entry doesn't have key so this method should not be called.
  DCHECK_EQ(EntryType::kParent, type());
  return key_;
}

Time MemEntryImpl::GetLastUsed() const {
  return last_used_;
}

int64_t MemEntryImpl::GetDataSize(int index) const {
  if (index < 0 || index >= kNumStreams)
    return 0;
  return data_[index].size();
}

int MemEntryImpl::ReadData(int index,
                           int64_t offset,
                           IOBuffer* buf,
                           int buf_len,
                           CompletionOnceCallback callback) {
  // TODO(crbug.com/391398191): Update the maximum to size_t max when it's
  // supported. `offset` must be within size_t range anyway so that the data
  // will be in-mmory.
  if (offset > std::numeric_limits<int32_t>::max()) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(net_log_, net::NetLogEventType::ENTRY_READ_DATA,
                              net::NetLogEventPhase::NONE,
                              net::ERR_INVALID_ARGUMENT);
    }
    return net::ERR_INVALID_ARGUMENT;
  }

  if (net_log_.IsCapturing()) {
    NetLogReadWriteData(net_log_, net::NetLogEventType::ENTRY_READ_DATA,
                        net::NetLogEventPhase::BEGIN, index, offset, buf_len,
                        false);
  }

  int result = InternalReadData(index, base::checked_cast<int32_t>(offset), buf,
                                buf_len);

  if (net_log_.IsCapturing()) {
    NetLogReadWriteComplete(net_log_, net::NetLogEventType::ENTRY_READ_DATA,
                            net::NetLogEventPhase::END, result);
  }
  return result;
}

int MemEntryImpl::WriteData(int index,
                            int64_t offset,
                            IOBuffer* buf,
                            int buf_len,
                            CompletionOnceCallback callback,
                            bool truncate) {
  // TODO(crbug.com/391398191): Update the maximum to size_t max when it's
  // supported. `offset` must be within size_t range anyway so that the data
  // will be in-mmory.
  if (offset > std::numeric_limits<int32_t>::max()) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(net_log_, net::NetLogEventType::ENTRY_READ_DATA,
                              net::NetLogEventPhase::NONE,
                              net::ERR_INVALID_ARGUMENT);
    }
    return net::ERR_INVALID_ARGUMENT;
  }

  if (net_log_.IsCapturing()) {
    NetLogReadWriteData(net_log_, net::NetLogEventType::ENTRY_WRITE_DATA,
                        net::NetLogEventPhase::BEGIN, index, offset, buf_len,
                        truncate);
  }

  int result = InternalWriteData(index, base::checked_cast<int32_t>(offset),
                                 buf, buf_len, truncate);

  if (net_log_.IsCapturing()) {
    NetLogReadWriteComplete(net_log_, net::NetLogEventType::ENTRY_WRITE_DATA,
                            net::NetLogEventPhase::END, result);
  }

  return result;
}

int MemEntryImpl::ReadSparseData(int64_t offset,
                                 IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback) {
  if (offset < 0 || buf_len < 0) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(net_log_, net::NetLogEventType::SPARSE_READ,
                              net::NetLogEventPhase::NONE,
                              net::ERR_INVALID_ARGUMENT);
    }

    return net::ERR_INVALID_ARGUMENT;
  }

  if (net_log_.IsCapturing()) {
    NetLogSparseOperation(net_log_, net::NetLogEventType::SPARSE_READ,
                          net::NetLogEventPhase::BEGIN, offset, buf_len);
  }

  // Ensure that offset + buf_len does not overflow. This ensures that
  // offset + io_buf->BytesConsumed() never overflows below.
  // The result of std::min is guaranteed to fit into int since buf_len did.
  size_t length = std::min(static_cast<int64_t>(buf_len),
                           std::numeric_limits<int64_t>::max() - offset);

  int result =
      InternalReadSparseData(base::checked_cast<uint64_t>(offset), buf, length);

  if (net_log_.IsCapturing())
    net_log_.EndEvent(net::NetLogEventType::SPARSE_READ);
  return result;
}

int MemEntryImpl::WriteSparseData(int64_t offset,
                                  IOBuffer* buf,
                                  int buf_len,
                                  CompletionOnceCallback callback) {
  if (offset < 0 || buf_len < 0 || !base::CheckAdd(offset, buf_len).IsValid()) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(net_log_, net::NetLogEventType::SPARSE_WRITE,
                              net::NetLogEventPhase::NONE,
                              net::ERR_INVALID_ARGUMENT);
    }

    return net::ERR_INVALID_ARGUMENT;
  }

  if (net_log_.IsCapturing()) {
    NetLogSparseOperation(net_log_, net::NetLogEventType::SPARSE_WRITE,
                          net::NetLogEventPhase::BEGIN, offset, buf_len);
  }

  int result =
      InternalWriteSparseData(base::checked_cast<uint64_t>(offset), buf,
                              base::checked_cast<size_t>(buf_len));
  if (net_log_.IsCapturing())
    net_log_.EndEvent(net::NetLogEventType::SPARSE_WRITE);
  return result;
}

RangeResult MemEntryImpl::GetAvailableRange(int64_t offset,
                                            int len,
                                            RangeResultCallback callback) {
  if (offset < 0 || len < 0) {
    if (net_log_.IsCapturing()) {
      NetLogReadWriteComplete(net_log_, net::NetLogEventType::SPARSE_GET_RANGE,
                              net::NetLogEventPhase::NONE,
                              net::ERR_INVALID_ARGUMENT);
    }

    return RangeResult(net::ERR_INVALID_ARGUMENT);
  }

  if (net_log_.IsCapturing()) {
    NetLogSparseOperation(net_log_, net::NetLogEventType::SPARSE_GET_RANGE,
                          net::NetLogEventPhase::BEGIN, offset, len);
  }

  // Truncate |len| to make sure that |offset + len| does not overflow.
  // This is OK since one can't write that far anyway.
  // The result of std::min is guaranteed to fit into int since |len| did.
  size_t length = std::min(static_cast<int64_t>(len),
                           std::numeric_limits<int64_t>::max() - offset);

  RangeResult result =
      InternalGetAvailableRange(base::checked_cast<uint64_t>(offset), length);
  if (net_log_.IsCapturing()) {
    net_log_.EndEvent(net::NetLogEventType::SPARSE_GET_RANGE, [&] {
      return CreateNetLogGetAvailableRangeResultParams(result);
    });
  }
  return result;
}

bool MemEntryImpl::CouldBeSparse() const {
  DCHECK_EQ(EntryType::kParent, type());
  return (children_.get() != nullptr);
}

net::Error MemEntryImpl::ReadyForSparseIO(CompletionOnceCallback callback) {
  return net::OK;
}

void MemEntryImpl::SetLastUsedTimeForTest(base::Time time) {
  last_used_ = time;
}

// ------------------------------------------------------------------------

MemEntryImpl::MemEntryImpl(base::WeakPtr<MemBackendImpl> backend,
                           const ::std::string& key,
                           int64_t child_id,
                           MemEntryImpl* parent,
                           net::NetLog* net_log)
    : key_(key),
      child_id_(child_id),
      parent_(parent),
      last_used_(MemBackendImpl::Now(backend)),
      backend_(backend) {
  backend_->OnEntryInserted(this);
  net_log_ = net::NetLogWithSource::Make(
      net_log, net::NetLogSourceType::MEMORY_CACHE_ENTRY);
  net_log_.BeginEvent(net::NetLogEventType::DISK_CACHE_MEM_ENTRY_IMPL,
                      [&] { return NetLogEntryCreationParams(this); });
}

MemEntryImpl::~MemEntryImpl() {
  if (backend_)
    backend_->ModifyStorageSize(-GetStorageSize());

  if (type() == EntryType::kParent) {
    if (children_) {
      EntryMap children;
      children_->swap(children);

      for (auto& it : children) {
        // Since |this| is stored in the map, it should be guarded against
        // double dooming, which will result in double destruction.
        if (it.second != this)
          it.second->Doom();
      }
    }
  } else {
    parent_->children_->erase(child_id_);
  }
  net_log_.EndEvent(net::NetLogEventType::DISK_CACHE_MEM_ENTRY_IMPL);
}

int MemEntryImpl::InternalReadData(int index, int offset, IOBuffer* buf,
                                   int buf_len) {
  DCHECK(type() == EntryType::kParent || index == kSparseData);

  if (index < 0 || index >= kNumStreams || offset < 0 || buf_len < 0) {
    return net::ERR_INVALID_ARGUMENT;
  }

  int entry_size = data_[index].size();
  if (offset >= entry_size || !buf_len) {
    return 0;
  }
  unsigned u_offset = static_cast<unsigned>(offset);

  int end_offset;
  if (!base::CheckAdd(offset, buf_len).AssignIfValid(&end_offset) ||
      end_offset > entry_size)
    buf_len = entry_size - offset;

  UpdateStateOnUse();
  buf->span().copy_prefix_from(
      base::as_byte_span(data_[index])
          .subspan(u_offset, base::checked_cast<size_t>(buf_len)));
  return buf_len;
}

int MemEntryImpl::InternalWriteData(int index, int offset, IOBuffer* buf,
                                    int buf_len, bool truncate) {
  DCHECK(type() == EntryType::kParent || index == kSparseData);
  if (!backend_)
    return net::ERR_INSUFFICIENT_RESOURCES;

  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  if (!buf && buf_len != 0) {
    return net::ERR_INVALID_ARGUMENT;
  }

  unsigned u_offset = static_cast<unsigned>(offset);
  unsigned u_buf_len = static_cast<unsigned>(buf_len);

  const int max_file_size = backend_->MaxFileSize();

  int end_offset;
  if (offset > max_file_size || buf_len > max_file_size ||
      !base::CheckAdd(offset, buf_len).AssignIfValid(&end_offset) ||
      end_offset > max_file_size) {
    return net::ERR_FAILED;
  }

  // Trim to the portion of the buffer we're actually asked to work on.
  // We need to be careful here since `buf` may be null if the length is 0;
  // this may still affect the file if it gets truncated or extended.
  base::span<uint8_t> to_write;
  if (buf) {
    to_write = buf->first(u_buf_len);
  }

  std::vector<char>& data = data_[index];
  const int old_data_size = base::checked_cast<int>(data.size());

  // Overwrite any data that fits inside the existing file.
  if (u_offset < data.size() && !to_write.empty()) {
    auto overwrite_chunk =
        to_write.first(std::min(data.size() - u_offset, to_write.size()));
    base::as_writable_byte_span(data).subspan(u_offset).copy_prefix_from(
        overwrite_chunk);
  }

  const int delta = end_offset - old_data_size;
  if (truncate && delta < 0) {
    // We permit reducing the size even if the storage size has been exceeded,
    // since it can only improve the situation. See https://crbug.com/331839344.
    backend_->ModifyStorageSize(delta);
    data.resize(end_offset);
  } else if (delta > 0) {
    backend_->ModifyStorageSize(delta);
    if (backend_->HasExceededStorageSize()) {
      backend_->ModifyStorageSize(-delta);
      return net::ERR_INSUFFICIENT_RESOURCES;
    }

    // Zero fill any hole.
    int current_size = old_data_size;
    if (current_size < offset) {
      data.resize(offset);
      current_size = offset;
    }
    // Append any data after the old end of the file.
    if (end_offset > current_size) {
      auto append_chunk =
          to_write.subspan(base::checked_cast<size_t>(current_size - offset));

      data.insert(data.end(), append_chunk.begin(), append_chunk.end());
    }
  }

  UpdateStateOnUse();

  return buf_len;
}

int MemEntryImpl::InternalReadSparseData(uint64_t offset,
                                         IOBuffer* buf,
                                         size_t buf_len) {
  DCHECK_EQ(EntryType::kParent, type());

  if (!InitSparseInfo())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  // We will keep using this buffer and adjust the offset in this buffer.
  scoped_refptr<net::DrainableIOBuffer> io_buf =
      base::MakeRefCounted<net::DrainableIOBuffer>(buf, buf_len);

  // Iterate until we have read enough.
  while (io_buf->BytesRemaining()) {
    MemEntryImpl* child = GetChild(offset + io_buf->BytesConsumed(), false);

    // No child present for that offset.
    if (!child)
      break;

    // We then need to prepare the child offset and len.
    size_t child_offset = ToChildOffset(offset + io_buf->BytesConsumed());

    // If we are trying to read from a position that the child entry has no data
    // we should stop.
    if (child_offset < child->child_first_pos_)
      break;
    if (net_log_.IsCapturing()) {
      NetLogSparseReadWrite(net_log_,
                            net::NetLogEventType::SPARSE_READ_CHILD_DATA,
                            net::NetLogEventPhase::BEGIN,
                            child->net_log_.source(), io_buf->BytesRemaining());
    }
    int ret =
        child->ReadData(kSparseData, child_offset, io_buf.get(),
                        io_buf->BytesRemaining(), CompletionOnceCallback());
    if (net_log_.IsCapturing()) {
      net_log_.EndEventWithNetErrorCode(
          net::NetLogEventType::SPARSE_READ_CHILD_DATA, ret);
    }

    // If we encounter an error in one entry, return immediately.
    if (ret < 0)
      return ret;
    else if (ret == 0)
      break;

    // Increment the counter by number of bytes read in the child entry.
    io_buf->DidConsume(ret);
  }

  UpdateStateOnUse();
  return io_buf->BytesConsumed();
}

int MemEntryImpl::InternalWriteSparseData(uint64_t offset,
                                          IOBuffer* buf,
                                          size_t buf_len) {
  DCHECK_EQ(EntryType::kParent, type());

  if (!InitSparseInfo())
    return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;

  // We can't generally do this without the backend since we need it to create
  // child entries.
  if (!backend_)
    return net::ERR_FAILED;

  scoped_refptr<net::DrainableIOBuffer> io_buf =
      base::MakeRefCounted<net::DrainableIOBuffer>(buf, buf_len);

  // This loop walks through child entries continuously starting from |offset|
  // and writes blocks of data (of maximum size kMaxChildEntrySize) into each
  // child entry until all |buf_len| bytes are written. The write operation can
  // start in the middle of an entry.
  while (io_buf->BytesRemaining()) {
    MemEntryImpl* child = GetChild(offset + io_buf->BytesConsumed(), true);
    size_t child_offset = ToChildOffset(offset + io_buf->BytesConsumed());

    // Find the right amount to write, this evaluates the remaining bytes to
    // write and remaining capacity of this child entry.
    size_t write_len = std::min(static_cast<size_t>(io_buf->BytesRemaining()),
                                kMaxChildEntrySize - child_offset);

    // Keep a record of the last byte position (exclusive) in the child.
    size_t data_size = child->GetDataSize(kSparseData);

    if (net_log_.IsCapturing()) {
      NetLogSparseReadWrite(
          net_log_, net::NetLogEventType::SPARSE_WRITE_CHILD_DATA,
          net::NetLogEventPhase::BEGIN, child->net_log_.source(), write_len);
    }

    // Always writes to the child entry. This operation may overwrite data
    // previously written.
    // TODO(hclam): if there is data in the entry and this write is not
    // continuous we may want to discard this write.
    int ret = child->WriteData(kSparseData, child_offset, io_buf.get(),
                               write_len, CompletionOnceCallback(), true);
    if (net_log_.IsCapturing()) {
      net_log_.EndEventWithNetErrorCode(
          net::NetLogEventType::SPARSE_WRITE_CHILD_DATA, ret);
    }
    if (ret < 0)
      return ret;
    else if (ret == 0)
      break;

    // Keep a record of the first byte position in the child if the write was
    // not aligned nor continuous. This is to enable witting to the middle
    // of an entry and still keep track of data off the aligned edge.
    if (data_size != child_offset)
      child->child_first_pos_ = child_offset;

    // Adjust the offset in the IO buffer.
    io_buf->DidConsume(ret);
  }

  UpdateStateOnUse();
  return io_buf->BytesConsumed();
}

RangeResult MemEntryImpl::InternalGetAvailableRange(uint64_t offset,
                                                    size_t len) {
  DCHECK_EQ(EntryType::kParent, type());

  if (!InitSparseInfo())
    return RangeResult(net::ERR_CACHE_OPERATION_NOT_SUPPORTED);

  net::Interval<uint64_t> requested(offset, offset + len);

  // Find the first relevant child, if any --- may have to skip over
  // one entry as it may be before the range (consider, for example,
  // if the request is for [2048, 10000), while [0, 1024) is a valid range
  // for the entry).
  EntryMap::const_iterator i = children_->lower_bound(ToChildIndex(offset));
  if (i != children_->cend() && !ChildInterval(i).Intersects(requested))
    ++i;
  net::Interval<uint64_t> found;
  if (i != children_->cend() &&
      requested.Intersects(ChildInterval(i), &found)) {
    // Found something relevant; now just need to expand this out if next
    // children are contiguous and relevant to the request.
    while (true) {
      ++i;
      net::Interval<uint64_t> relevant_in_next_child;
      if (i == children_->cend() ||
          !requested.Intersects(ChildInterval(i), &relevant_in_next_child) ||
          relevant_in_next_child.min() != found.max()) {
        break;
      }

      found.SpanningUnion(relevant_in_next_child);
    }

    return RangeResult(found.min(), found.Length());
  }

  return RangeResult(offset, 0);
}

bool MemEntryImpl::InitSparseInfo() {
  DCHECK_EQ(EntryType::kParent, type());

  if (!children_) {
    // If we already have some data in sparse stream but we are being
    // initialized as a sparse entry, we should fail.
    if (GetDataSize(kSparseData))
      return false;
    children_ = std::make_unique<EntryMap>();

    // The parent entry stores data for the first block, so save this object to
    // index 0.
    (*children_)[0] = this;
  }
  return true;
}

MemEntryImpl* MemEntryImpl::GetChild(uint64_t offset, bool create) {
  DCHECK_EQ(EntryType::kParent, type());
  uint64_t index = ToChildIndex(offset);
  auto i = children_->find(index);
  if (i != children_->end())
    return i->second;
  if (create)
    return new MemEntryImpl(backend_, index, this, net_log_.net_log());
  return nullptr;
}

net::Interval<uint64_t> MemEntryImpl::ChildInterval(
    MemEntryImpl::EntryMap::const_iterator i) {
  DCHECK(i != children_->cend());
  const MemEntryImpl* child = i->second;
  // The valid range in child is [child_first_pos_, DataSize), since the child
  // entry ops just use standard disk_cache::Entry API, so DataSize is
  // not aware of any hole in the beginning.
  int64_t child_responsibility_start = (i->first) * kMaxChildEntrySize;
  return net::Interval<uint64_t>(
      child_responsibility_start + child->child_first_pos_,
      child_responsibility_start + child->GetDataSize(kSparseData));
}

void MemEntryImpl::Compact() {
  // Stream 0 should already be fine since it's written out in a single WriteData().
  data_[1].shrink_to_fit();
  data_[2].shrink_to_fit();
}

}  // namespace disk_cache
