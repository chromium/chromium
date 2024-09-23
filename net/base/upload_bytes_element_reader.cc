// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_bytes_element_reader.h"

#include "base/check_op.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

UploadBytesElementReader::UploadBytesElementReader(
    base::span<const uint8_t> bytes)
    : bytes_(bytes) {}

UploadBytesElementReader::~UploadBytesElementReader() = default;

const UploadBytesElementReader*
UploadBytesElementReader::AsBytesReader() const {
  return this;
}

int UploadBytesElementReader::Init(CompletionOnceCallback callback) {
  offset_ = 0;
  return OK;
}

uint64_t UploadBytesElementReader::GetContentLength() const {
  return bytes_.size();
}

uint64_t UploadBytesElementReader::BytesRemaining() const {
  return bytes_.size() - offset_;
}

bool UploadBytesElementReader::IsInMemory() const {
  return true;
}

int UploadBytesElementReader::Read(IOBuffer* buf,
                                   int buf_length,
                                   CompletionOnceCallback callback) {
  DCHECK_LT(0, buf_length);

  base::span<const uint8_t> bytes_to_read = bytes_.subspan(
      offset_, std::min(BytesRemaining(), static_cast<uint64_t>(buf_length)));
  if (!bytes_to_read.empty()) {
    buf->span().copy_prefix_from(bytes_to_read);
  }

  offset_ += bytes_to_read.size();
  return bytes_to_read.size();
}

UploadOwnedBytesElementReader::UploadOwnedBytesElementReader(
    std::vector<char>* data)
    : UploadBytesElementReader(base::as_byte_span(*data)) {
  data_.swap(*data);
}

UploadOwnedBytesElementReader::~UploadOwnedBytesElementReader() = default;

std::unique_ptr<UploadOwnedBytesElementReader>
UploadOwnedBytesElementReader::CreateWithString(const std::string& string) {
  std::vector<char> data(string.begin(), string.end());
  return std::make_unique<UploadOwnedBytesElementReader>(&data);
}

}  // namespace net
