// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/elements_upload_data_stream.h"

#include "base/bind.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"

namespace net {

ElementsUploadDataStream::ElementsUploadDataStream(
    std::vector<std::unique_ptr<UploadElementReader>> element_readers,
    int64_t identifier)
    : UploadDataStream(false, identifier),
      element_readers_(std::move(element_readers)),
      element_index_(0),
      read_error_(OK) {}

ElementsUploadDataStream::~ElementsUploadDataStream() = default;

std::unique_ptr<UploadDataStream> ElementsUploadDataStream::CreateWithReader(
    std::unique_ptr<UploadElementReader> reader,
    int64_t identifier) {
  std::vector<std::unique_ptr<UploadElementReader>> readers;
  readers.push_back(std::move(reader));
  return std::unique_ptr<UploadDataStream>(
      new ElementsUploadDataStream(std::move(readers), identifier));
}

int ElementsUploadDataStream::InitInternal(const NetLogWithSource& net_log) {
  return InitElements(0);
}

int ElementsUploadDataStream::ReadInternal(
    IOBuffer* buf,
    int buf_len) {
  DCHECK_GT(buf_len, 0);
  return ReadElements(base::MakeRefCounted<DrainableIOBuffer>(buf, buf_len));
}

bool ElementsUploadDataStream::IsInMemory() const {
  for (const std::unique_ptr<UploadElementReader>& it : element_readers_) {
    if (!it->IsInMemory())
      return false;
  }
  return true;
}

const std::vector<std::unique_ptr<UploadElementReader>>*
ElementsUploadDataStream::GetElementReaders() const {
  return &element_readers_;
}

void ElementsUploadDataStream::ResetInternal() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  read_error_ = OK;
  element_index_ = 0;
}

int ElementsUploadDataStream::InitElements(size_t start_index) {
  // Call Init() for all elements.
  for (size_t i = start_index; i < element_readers_.size(); ++i) {
    UploadElementReader* reader = element_readers_[i].get();
    // When new_result is ERR_IO_PENDING, InitInternal() will be called
    // with start_index == i + 1 when reader->Init() finishes.
    int result = reader->Init(
        base::BindOnce(&ElementsUploadDataStream::OnInitElementCompleted,
                       weak_ptr_factory_.GetWeakPtr(), i));
    DCHECK(result != ERR_IO_PENDING || !reader->IsInMemory());
    DCHECK_LE(result, OK);
    if (result != OK)
      return result;
  }

  uint64_t total_size = 0;
  for (const std::unique_ptr<UploadElementReader>& it : element_readers_) {
    total_size += it->GetContentLength();
  }
  SetSize(total_size);
  return OK;
}

void ElementsUploadDataStream::OnInitElementCompleted(size_t index,
                                                      int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  // Check the last result.
  if (result == OK)
    result = InitElements(index + 1);

  if (result != ERR_IO_PENDING)
    OnInitCompleted(result);
}

int ElementsUploadDataStream::ReadElements(
    const scoped_refptr<DrainableIOBuffer>& buf) {
  while (read_error_ == OK && element_index_ < element_readers_.size()) {
    UploadElementReader* reader = element_readers_[element_index_].get();

    if (reader->BytesRemaining() == 0) {
      ++element_index_;
      continue;
    }

    if (buf->BytesRemaining() == 0)
      break;

    int result = reader->Read(
        buf.get(), buf->BytesRemaining(),
        base::BindOnce(&ElementsUploadDataStream::OnReadElementCompleted,
                       weak_ptr_factory_.GetWeakPtr(), buf));
    if (result == ERR_IO_PENDING)
      return ERR_IO_PENDING;
    ProcessReadResult(buf, result);
  }

  if (buf->BytesConsumed() > 0)
    return buf->BytesConsumed();

  return read_error_;
}

void ElementsUploadDataStream::OnReadElementCompleted(
    const scoped_refptr<DrainableIOBuffer>& buf,
    int result) {
  ProcessReadResult(buf, result);

  result = ReadElements(buf);
  if (result != ERR_IO_PENDING)
    OnReadCompleted(result);
}

void ElementsUploadDataStream::ProcessReadResult(
    const scoped_refptr<DrainableIOBuffer>& buf,
    int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(!read_error_);

  if (result >= 0) {
    buf->DidConsume(result);
  } else {
    read_error_ = result;
  }
}

}  // namespace net
