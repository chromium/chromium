// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/blob/blob_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace storage {
namespace {

bool IsFileType(BlobDataItem::Type type) {
  switch (type) {
    case BlobDataItem::Type::kFile:
    case BlobDataItem::Type::kFileFilesystem:
      return true;
    default:
      return false;
  }
}

int ConvertBlobErrorToNetError(BlobStatus reason) {
  switch (reason) {
    case BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS:
      return net::ERR_FAILED;
    case BlobStatus::ERR_OUT_OF_MEMORY:
      return net::ERR_OUT_OF_MEMORY;
    case BlobStatus::ERR_FILE_WRITE_FAILED:
      return net::ERR_FILE_NO_SPACE;
    case BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT:
      return net::ERR_UNEXPECTED;
    case BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING:
      return net::ERR_UNEXPECTED;
    case BlobStatus::ERR_REFERENCED_BLOB_BROKEN:
      return net::ERR_INVALID_HANDLE;
    case BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE:
      return net::ERR_INVALID_HANDLE;
    case BlobStatus::DONE:
    case BlobStatus::PENDING_QUOTA:
    case BlobStatus::PENDING_TRANSPORT:
    case BlobStatus::PENDING_REFERENCED_BLOBS:
    case BlobStatus::PENDING_CONSTRUCTION:
      NOTREACHED();
  }
  NOTREACHED();
}
}  // namespace

BlobReader::FileStreamReaderProvider::~FileStreamReaderProvider() = default;

BlobReader::BlobReader(const BlobDataHandle* blob_handle)
    : file_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})),
      net_error_(net::OK) {
  if (blob_handle) {
    if (blob_handle->IsBroken()) {
      net_error_ = ConvertBlobErrorToNetError(blob_handle->GetBlobStatus());
    } else {
      blob_handle_ = std::make_unique<BlobDataHandle>(*blob_handle);
    }
  }
}

BlobReader::~BlobReader() = default;

BlobReader::Status BlobReader::CalculateSize(net::CompletionOnceCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!total_size_calculated_);
  DCHECK(size_callback_.is_null());
  if (!blob_handle_.get())
    return ReportError(net::ERR_FILE_NOT_FOUND);

  if (blob_handle_->IsBroken()) {
    return ReportError(
        ConvertBlobErrorToNetError(blob_handle_->GetBlobStatus()));
  }

  if (blob_handle_->IsBeingBuilt()) {
    blob_handle_->RunOnConstructionComplete(
        base::BindOnce(&BlobReader::AsyncCalculateSize,
                       weak_factory_.GetWeakPtr(), std::move(done)));
    return Status::IO_PENDING;
  }
  blob_data_ = blob_handle_->CreateSnapshot();
  return CalculateSizeImpl(&done);
}

bool BlobReader::has_side_data() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!blob_data_.get())
    return false;
  const auto& items = blob_data_->items();
  if (items.size() != 1)
    return false;
  const BlobDataItem& item = *items.at(0);
  if (item.type() != BlobDataItem::Type::kReadableDataHandle)
    return false;
  return item.data_handle()->GetSideDataSize() > 0;
}

void BlobReader::ReadSideData(StatusCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  side_data_.reset();
  if (!has_side_data()) {
    std::move(done).Run(ReportError(net::ERR_FILE_NOT_FOUND));
    return;
  }
  BlobDataItem* item = blob_data_->items()[0].get();
  const int side_data_size = item->data_handle()->GetSideDataSize();
  item->data_handle()->ReadSideData(
      base::BindOnce(&BlobReader::DidReadSideData, weak_factory_.GetWeakPtr(),
                     std::move(done), side_data_size));
}

std::optional<mojo_base::BigBuffer> BlobReader::TakeSideData() {
  return std::move(side_data_);
}

void BlobReader::DidReadSideData(StatusCallback done,
                                 int expected_size,
                                 int result,
                                 mojo_base::BigBuffer data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result >= 0) {
    DCHECK_EQ(expected_size, result);
    DCHECK_EQ(static_cast<size_t>(expected_size), data.size());
    RecordBytesReadFromDataHandle(/* item_index= */ 0, result);
    side_data_ = std::move(data);
    std::move(done).Run(Status::DONE);
    return;
  }
  std::move(done).Run(ReportError(result));
}

BlobReader::Status BlobReader::SetReadRange(uint64_t offset, uint64_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!blob_handle_.get())
    return ReportError(net::ERR_FILE_NOT_FOUND);
  if (blob_handle_->IsBroken()) {
    return ReportError(
        ConvertBlobErrorToNetError(blob_handle_->GetBlobStatus()));
  }
  if (!total_size_calculated_)
    return ReportError(net::ERR_UNEXPECTED);
  if (offset + length > total_size_)
    return ReportError(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);

  // Skip the initial items that are not in the range.
  remaining_bytes_ = length;
  const auto& items = blob_data_->items();
  for (current_item_index_ = 0;
       current_item_index_ < items.size() &&
       offset >= item_length_list_[current_item_index_];
       ++current_item_index_) {
    offset -= item_length_list_[current_item_index_];
  }

  // Set the offset that need to jump to for the first item in the range.
  current_item_offset_ = offset;
  if (current_item_offset_ == 0)
    return Status::DONE;

  // Adjust the offset of the first stream if it is a file or data handle.
  const BlobDataItem& item = *items.at(current_item_index_);
  if (IsFileType(item.type())) {
    SetFileReaderAtIndex(current_item_index_,
                         CreateFileStreamReader(item, offset));
  }
  if (item.type() == BlobDataItem::Type::kReadableDataHandle) {
    SetDataPipeAtIndex(current_item_index_, CreateDataPipe(item, offset));
  }
  return Status::DONE;
}

BlobReader::Status BlobReader::Read(net::IOBuffer* buffer,
                                    size_t dest_size,
                                    int* bytes_read,
                                    net::CompletionOnceCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0ul);
  DCHECK(read_callback_.is_null());

  *bytes_read = 0;
  if (!blob_data_.get())
    return ReportError(net::ERR_FILE_NOT_FOUND);
  if (!total_size_calculated_)
    return ReportError(net::ERR_UNEXPECTED);

  // Bail out immediately if we encountered an error.
  if (net_error_ != net::OK)
    return Status::NET_ERROR;

  DCHECK_GE(dest_size, 0ul);
  if (remaining_bytes_ < static_cast<uint64_t>(dest_size))
    dest_size = static_cast<int>(remaining_bytes_);

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size) {
    *bytes_read = 0;
    return Status::DONE;
  }

  // Keep track of the buffer.
  DCHECK(!read_buf_.get());
  read_buf_ = base::MakeRefCounted<net::DrainableIOBuffer>(buffer, dest_size);

  Status status = ReadLoop(bytes_read);
  if (status == Status::IO_PENDING)
    read_callback_ = std::move(done);
  return status;
}

bool BlobReader::IsSingleMojoDataItem() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(total_size_calculated_);
  if (!blob_data_.get())
    return false;
  if (blob_data_->items().size() != 1)
    return false;
  if (blob_data_->items()[0]->type() != BlobDataItem::Type::kReadableDataHandle)
    return false;
  return true;
}

void BlobReader::ReadSingleMojoDataItem(
    mojo::ScopedDataPipeProducerHandle producer,
    net::CompletionOnceCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsSingleMojoDataItem());

  // Read the set range of this single data item.
  auto item = blob_data_->items()[0];
  item->data_handle()->Read(std::move(producer),
                            item->offset() + current_item_offset_,
                            remaining_bytes_, std::move(done));
}

void BlobReader::Kill() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DeleteItemReaders();
  weak_factory_.InvalidateWeakPtrs();
}

bool BlobReader::IsInMemory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (blob_handle_ && blob_handle_->IsBeingBuilt()) {
    return false;
  }
  if (!blob_data_.get()) {
    return true;
  }
  for (const auto& item : blob_data_->items()) {
    if (item->type() != BlobDataItem::Type::kBytes) {
      return false;
    }
  }
  return true;
}

void BlobReader::InvalidateCallbacksAndDone(int net_error,
                                            net::CompletionOnceCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net_error_ = net_error;
  weak_factory_.InvalidateWeakPtrs();
  size_callback_.Reset();
  read_callback_.Reset();
  read_buf_ = nullptr;
  std::move(done).Run(net_error);
}

BlobReader::Status BlobReader::ReportError(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net_error_ = net_error;
  return Status::NET_ERROR;
}

void BlobReader::AsyncCalculateSize(net::CompletionOnceCallback done,
                                    BlobStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (BlobStatusIsError(status)) {
    InvalidateCallbacksAndDone(ConvertBlobErrorToNetError(status),
                               std::move(done));
    return;
  }
  DCHECK(!blob_handle_->IsBroken()) << "Callback should have returned false.";
  blob_data_ = blob_handle_->CreateSnapshot();
  Status size_status = CalculateSizeImpl(&done);
  switch (size_status) {
    case Status::NET_ERROR:
      InvalidateCallbacksAndDone(net_error_, std::move(done));
      return;
    case Status::DONE:
      std::move(done).Run(net::OK);
      return;
    case Status::IO_PENDING:
      // CalculateSizeImpl() should have taken ownership of |done|.
      DCHECK(!done);
      return;
  }
}

BlobReader::Status BlobReader::CalculateSizeImpl(
    net::CompletionOnceCallback* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!total_size_calculated_);
  DCHECK(size_callback_.is_null());

  if (!blob_data_) {
    return ReportError(net::ERR_UNEXPECTED);
  }

  net_error_ = net::OK;
  total_size_ = 0;
  const auto& items = blob_data_->items();
  item_length_list_.resize(items.size());
  pending_get_file_info_count_ = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    const BlobDataItem& item = *items.at(i);
    if (!IsFileType(item.type())) {
      if (!AddItemLength(i, item.length()))
        return ReportError(net::ERR_INSUFFICIENT_RESOURCES);
      continue;
    }
    ++pending_get_file_info_count_;
    FileStreamReader* const reader = GetOrCreateFileReaderAtIndex(i);
    if (!reader)
      return ReportError(net::ERR_FILE_NOT_FOUND);

    int64_t length_output = reader->GetLength(base::BindOnce(
        &BlobReader::DidGetFileItemLength, weak_factory_.GetWeakPtr(), i));
    if (length_output == net::ERR_IO_PENDING)
      continue;
    if (length_output < 0)
      return ReportError(length_output);

    // We got the length right away
    --pending_get_file_info_count_;
    uint64_t resolved_length;
    if (!ResolveFileItemLength(item, length_output, &resolved_length))
      return ReportError(net::ERR_FAILED);
    if (!AddItemLength(i, resolved_length))
      return ReportError(net::ERR_FILE_TOO_BIG);
  }

  if (pending_get_file_info_count_ == 0) {
    DidCountSize();
    return Status::DONE;
  }
  // Note: We only set the callback if we know that we're an async operation.
  size_callback_ = std::move(*done);
  return Status::IO_PENDING;
}

bool BlobReader::AddItemLength(size_t index, uint64_t item_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (item_length > std::numeric_limits<uint64_t>::max() - total_size_)
    return false;

  // Cache the size and add it to the total size.
  DCHECK_LT(index, item_length_list_.size());
  item_length_list_[index] = item_length;
  total_size_ += item_length;
  return true;
}

bool BlobReader::ResolveFileItemLength(const BlobDataItem& item,
                                       int64_t total_length,
                                       uint64_t* output_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsFileType(item.type()));
  DCHECK(output_length);
  uint64_t file_length = total_length;
  uint64_t item_offset = item.offset();
  uint64_t item_length = item.length();
  if (item_offset > file_length)
    return false;

  uint64_t max_length = file_length - item_offset;

  // If item length is undefined, then we need to use the file size being
  // resolved in the real time.
  if (item_length == std::numeric_limits<uint64_t>::max()) {
    item_length = max_length;
  } else if (item_length > max_length) {
    return false;
  }

  *output_length = item_length;
  return true;
}

void BlobReader::DidGetFileItemLength(size_t index, int64_t result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do nothing if we have encountered an error.
  if (net_error_)
    return;

  if (result < 0) {
    InvalidateCallbacksAndDone(result, std::move(size_callback_));
    return;
  }

  const auto& items = blob_data_->items();
  DCHECK_LT(index, items.size());
  const BlobDataItem& item = *items.at(index);
  uint64_t length;
  if (!ResolveFileItemLength(item, result, &length)) {
    InvalidateCallbacksAndDone(net::ERR_FAILED, std::move(size_callback_));
    return;
  }
  if (!AddItemLength(index, length)) {
    InvalidateCallbacksAndDone(net::ERR_FILE_TOO_BIG,
                               std::move(size_callback_));
    return;
  }

  if (--pending_get_file_info_count_ == 0)
    DidCountSize();
}

void BlobReader::DidCountSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!net_error_);
  total_size_calculated_ = true;
  remaining_bytes_ = total_size_;
  // This is set only if we're async.
  if (!size_callback_.is_null())
    std::move(size_callback_).Run(net::OK);
}

BlobReader::Status BlobReader::ReadLoop(int* bytes_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Read until we encounter an error or could not get the data immediately.
  while (remaining_bytes_ > 0 && read_buf_->BytesRemaining() > 0) {
    Status read_status = ReadItem();
    if (read_status == Status::DONE)
      continue;
    return read_status;
  }

  *bytes_read = BytesReadCompleted();
  return Status::DONE;
}

BlobReader::Status BlobReader::ReadItem() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Are we done with reading all the blob data?
  if (remaining_bytes_ == 0)
    return Status::DONE;

  const auto& items = blob_data_->items();
  // If we get to the last item but still expect something to read, bail out
  // since something is wrong.
  if (current_item_index_ >= items.size())
    return ReportError(net::ERR_UNEXPECTED);

  // Compute the bytes to read for current item.
  int bytes_to_read = ComputeBytesToRead();

  // If nothing to read for current item, advance to next item.
  if (bytes_to_read == 0) {
    AdvanceItem();
    return Status::DONE;
  }

  // Do the reading.
  BlobDataItem& item = *items.at(current_item_index_);
  if (item.type() == BlobDataItem::Type::kBytes) {
    ReadBytesItem(item, bytes_to_read);
    return Status::DONE;
  }
  if (item.type() == BlobDataItem::Type::kReadableDataHandle)
    return ReadReadableDataHandle(item, bytes_to_read);
  if (!IsFileType(item.type())) {
    NOTREACHED();
  }
  FileStreamReader* const reader =
      GetOrCreateFileReaderAtIndex(current_item_index_);
  if (!reader)
    return ReportError(net::ERR_FILE_NOT_FOUND);
  return ReadFileItem(reader, bytes_to_read, item.file_access());
}

void BlobReader::AdvanceItem() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Close any files or data pipes.
  DeleteItemReaders();

  // Advance to the next item.
  current_item_index_++;
  current_item_offset_ = 0;
}

void BlobReader::AdvanceBytesRead(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(result, 0);

  // Do we finish reading the current item?
  current_item_offset_ += result;
  if (current_item_offset_ == item_length_list_[current_item_index_])
    AdvanceItem();

  // Subtract the remaining bytes.
  remaining_bytes_ -= result;
  DCHECK_GE(remaining_bytes_, 0ul);

  // Adjust the read buffer.
  read_buf_->DidConsume(result);
  DCHECK_GE(read_buf_->BytesRemaining(), 0);
}

void BlobReader::ReadBytesItem(const BlobDataItem& item, int bytes_to_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("Blob", "BlobReader::ReadBytesItem", "uuid", blob_data_->uuid());
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);

  memcpy(read_buf_->data(),
         item.bytes().data() + item.offset() + current_item_offset_,
         bytes_to_read);

  AdvanceBytesRead(bytes_to_read);
}

BlobReader::Status BlobReader::ReadFileItem(
    FileStreamReader* reader,
    int bytes_to_read,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!io_pending_)
      << "Can't begin IO while another IO operation is pending.";
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);
  DCHECK(reader);
  const int result = reader->Read(
      read_buf_.get(), bytes_to_read,
      base::BindOnce(&BlobReader::DidReadFile, weak_factory_.GetWeakPtr()));
  if (result >= 0) {
    AdvanceBytesRead(result);
    return Status::DONE;
  }
  if (result == net::ERR_IO_PENDING) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("Blob", "BlobReader::ReadFileItem",
                                      TRACE_ID_LOCAL(this), "uuid",
                                      blob_data_->uuid());
    io_pending_ = true;
    return Status::IO_PENDING;
  }
  return ReportError(result);
}

void BlobReader::DidReadFile(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("Blob", "BlobReader::ReadFileItem",
                                  TRACE_ID_LOCAL(this), "uuid",
                                  blob_data_->uuid());
  DidReadItem(result);
}

void BlobReader::ContinueAsyncReadLoop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int bytes_read = 0;
  Status read_status = ReadLoop(&bytes_read);
  switch (read_status) {
    case Status::DONE:
      std::move(read_callback_).Run(bytes_read);
      return;
    case Status::NET_ERROR:
      InvalidateCallbacksAndDone(net_error_, std::move(read_callback_));
      return;
    case Status::IO_PENDING:
      return;
  }
}

void BlobReader::DeleteItemReaders() {
  SetFileReaderAtIndex(current_item_index_,
                       std::unique_ptr<FileStreamReader>());
  SetDataPipeAtIndex(current_item_index_,
                     std::unique_ptr<network::DataPipeToSourceStream>());
}

BlobReader::Status BlobReader::ReadReadableDataHandle(const BlobDataItem& item,
                                                      int bytes_to_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!io_pending_)
      << "Can't begin IO while another IO operation is pending.";
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);
  DCHECK_EQ(item.type(), BlobDataItem::Type::kReadableDataHandle);

  network::DataPipeToSourceStream* const pipe =
      GetOrCreateDataPipeAtIndex(current_item_index_);
  if (!pipe)
    return ReportError(net::ERR_UNEXPECTED);

  int result = pipe->Read(read_buf_.get(), bytes_to_read,
                          base::BindOnce(&BlobReader::DidReadReadableDataHandle,
                                         weak_factory_.GetWeakPtr()));
  if (result >= 0) {
    AdvanceBytesRead(result);
    return Status::DONE;
  }
  if (result == net::ERR_IO_PENDING) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "Blob", "BlobReader::ReadReadableDataHandle", TRACE_ID_LOCAL(this),
        "uuid", blob_data_->uuid());
    io_pending_ = true;
    return Status::IO_PENDING;
  }
  return ReportError(result);
}

void BlobReader::DidReadReadableDataHandle(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("Blob", "BlobReader::ReadReadableDataHandle",
                                  TRACE_ID_LOCAL(this), "uuid",
                                  blob_data_->uuid());
  RecordBytesReadFromDataHandle(current_item_index_, result);
  DidReadItem(result);
}

void BlobReader::DidReadItem(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_pending_) << "Asynchronous IO completed while IO wasn't pending?";
  io_pending_ = false;
  if (result <= 0) {
    InvalidateCallbacksAndDone(result, std::move(read_callback_));
    return;
  }
  AdvanceBytesRead(result);
  ContinueAsyncReadLoop();
}

int BlobReader::BytesReadCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int bytes_read = read_buf_->BytesConsumed();
  read_buf_ = nullptr;
  return bytes_read;
}

int BlobReader::ComputeBytesToRead() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint64_t current_item_length = item_length_list_[current_item_index_];

  uint64_t item_remaining = current_item_length - current_item_offset_;
  uint64_t buf_remaining = read_buf_->BytesRemaining();
  uint64_t max_int_value = std::numeric_limits<int>::max();
  // Here we make sure we don't overflow 'max int'.
  uint64_t min = std::min(
      {item_remaining, buf_remaining, remaining_bytes_, max_int_value});

  return static_cast<int>(min);
}

FileStreamReader* BlobReader::GetOrCreateFileReaderAtIndex(size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& items = blob_data_->items();
  DCHECK_LT(index, items.size());
  const BlobDataItem& item = *items.at(index);
  if (!IsFileType(item.type()))
    return nullptr;
  auto it = index_to_reader_.find(index);
  if (it != index_to_reader_.end()) {
    DCHECK(it->second);
    return it->second.get();
  }
  std::unique_ptr<FileStreamReader> reader = CreateFileStreamReader(item, 0);
  FileStreamReader* ret_value = reader.get();
  if (!ret_value)
    return nullptr;
  index_to_reader_[index] = std::move(reader);
  return ret_value;
}

std::unique_ptr<FileStreamReader> BlobReader::CreateFileStreamReader(
    const BlobDataItem& item,
    uint64_t additional_offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsFileType(item.type()));

  switch (item.type()) {
    case BlobDataItem::Type::kFile:
      if (file_stream_provider_for_testing_) {
        return file_stream_provider_for_testing_->CreateForLocalFile(
            file_task_runner_.get(), item.path(),
            item.offset() + additional_offset,
            item.expected_modification_time());
      }
      return FileStreamReader::CreateForLocalFile(
          file_task_runner_.get(), item.path(),
          item.offset() + additional_offset, item.expected_modification_time(),
          item.file_access());
    case BlobDataItem::Type::kFileFilesystem: {
      int64_t max_bytes_to_read =
          item.length() == std::numeric_limits<uint64_t>::max()
              ? kMaximumLength
              : item.length() - additional_offset;
      if (file_stream_provider_for_testing_) {
        return file_stream_provider_for_testing_->CreateFileStreamReader(
            item.filesystem_url().ToGURL(), item.offset() + additional_offset,
            max_bytes_to_read, item.expected_modification_time());
      }
      return item.file_system_context()->CreateFileStreamReader(
          item.filesystem_url(), item.offset() + additional_offset,
          max_bytes_to_read, item.expected_modification_time(),
          item.file_access());
    }
    case BlobDataItem::Type::kBytes:
    case BlobDataItem::Type::kBytesDescription:
    case BlobDataItem::Type::kReadableDataHandle:
      break;
  }

  NOTREACHED();
}

void BlobReader::SetFileReaderAtIndex(
    size_t index,
    std::unique_ptr<FileStreamReader> reader) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (reader)
    index_to_reader_[index] = std::move(reader);
  else
    index_to_reader_.erase(index);
}

network::DataPipeToSourceStream* BlobReader::GetOrCreateDataPipeAtIndex(
    size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& items = blob_data_->items();
  DCHECK_LT(index, items.size());
  const BlobDataItem& item = *items.at(index);
  if (item.type() != BlobDataItem::Type::kReadableDataHandle)
    return nullptr;
  auto it = index_to_pipe_.find(index);
  if (it != index_to_pipe_.end()) {
    DCHECK(it->second);
    return it->second.get();
  }
  auto pipe = CreateDataPipe(item, 0);
  auto* ret_value = pipe.get();
  index_to_pipe_[index] = std::move(pipe);
  return ret_value;
}

void BlobReader::SetDataPipeAtIndex(
    size_t index,
    std::unique_ptr<network::DataPipeToSourceStream> pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pipe)
    index_to_pipe_[index] = std::move(pipe);
  else
    index_to_pipe_.erase(index);
}

std::unique_ptr<network::DataPipeToSourceStream> BlobReader::CreateDataPipe(
    const BlobDataItem& item,
    uint64_t additional_offset) {
  DCHECK_EQ(item.type(), BlobDataItem::Type::kReadableDataHandle);

  uint64_t blob_size = item.length();
  uint64_t max_bytes_to_read = blob_size - additional_offset;
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      blink::BlobUtils::GetDataPipeCapacity(max_bytes_to_read);

  MojoResult result = mojo::CreateDataPipe(&options, producer, consumer);

  if (result != MOJO_RESULT_OK)
    return nullptr;

  auto adapter =
      std::make_unique<network::DataPipeToSourceStream>(std::move(consumer));
  item.data_handle()->Read(
      std::move(producer), additional_offset + item.offset(), max_bytes_to_read,
      base::BindOnce(
          [](base::WeakPtr<BlobReader> reader, int result) {
            if (!reader || result >= 0)
              return;
            reader->InvalidateCallbacksAndDone(
                result, std::move(reader->read_callback_));
          },
          weak_factory_.GetWeakPtr()));
  return adapter;
}

void BlobReader::RecordBytesReadFromDataHandle(int item_index, int result) {
  const auto& items = blob_data_->items();
  BlobDataItem& item = *items.at(item_index);
  DCHECK_EQ(item.type(), BlobDataItem::Type::kReadableDataHandle);
}

}  // namespace storage
