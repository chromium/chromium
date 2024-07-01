// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/shared_dictionary/shared_dictionary_header_checker_source_stream.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"

namespace net {
namespace {

static constexpr unsigned char kCompressionTypeBrotliSignature[] = {0xff, 0x44,
                                                                    0x43, 0x42};
static constexpr unsigned char kCompressionTypeZstdSignature[] = {
    0x5e, 0x2a, 0x4d, 0x18, 0x20, 0x00, 0x00, 0x00};
static constexpr size_t kCompressionTypeBrotliSignatureSize =
    sizeof(kCompressionTypeBrotliSignature);
static constexpr size_t kCompressionTypeZstdSignatureSize =
    sizeof(kCompressionTypeZstdSignature);
static constexpr int kCompressionDictionaryHashSize = 32;
static_assert(sizeof(SHA256HashValue) == kCompressionDictionaryHashSize,
              "kCompressionDictionaryHashSize mismatch");
static constexpr int kCompressionTypeBrotliHeaderSize =
    kCompressionTypeBrotliSignatureSize + kCompressionDictionaryHashSize;
static constexpr int kCompressionTypeZstdHeaderSize =
    kCompressionTypeZstdSignatureSize + kCompressionDictionaryHashSize;

size_t GetSignatureSize(SharedDictionaryHeaderCheckerSourceStream::Type type) {
  switch (type) {
    case SharedDictionaryHeaderCheckerSourceStream::Type::
        kDictionaryCompressedBrotli:
      return kCompressionTypeBrotliSignatureSize;
    case SharedDictionaryHeaderCheckerSourceStream::Type::
        kDictionaryCompressedZstd:
      return kCompressionTypeZstdSignatureSize;
  }
}

size_t GetHeaderSize(SharedDictionaryHeaderCheckerSourceStream::Type type) {
  switch (type) {
    case SharedDictionaryHeaderCheckerSourceStream::Type::
        kDictionaryCompressedBrotli:
      return kCompressionTypeBrotliHeaderSize;
    case SharedDictionaryHeaderCheckerSourceStream::Type::
        kDictionaryCompressedZstd:
      return kCompressionTypeZstdHeaderSize;
  }
}

base::span<const unsigned char> GetExpectedSignature(
    SharedDictionaryHeaderCheckerSourceStream::Type type) {
  switch (type) {
    case SharedDictionaryHeaderCheckerSourceStream::Type::
        kDictionaryCompressedBrotli:
      return kCompressionTypeBrotliSignature;
    case SharedDictionaryHeaderCheckerSourceStream::Type::
        kDictionaryCompressedZstd:
      return kCompressionTypeZstdSignature;
  }
}

}  // namespace

SharedDictionaryHeaderCheckerSourceStream::
    SharedDictionaryHeaderCheckerSourceStream(
        std::unique_ptr<SourceStream> upstream,
        Type type,
        const SHA256HashValue& dictionary_hash)
    : SourceStream(SourceStream::TYPE_NONE),
      upstream_(std::move(upstream)),
      type_(type),
      dictionary_hash_(dictionary_hash),
      head_read_buffer_(base::MakeRefCounted<GrowableIOBuffer>()) {
  head_read_buffer_->SetCapacity(GetHeaderSize(type_));
  ReadHeader();
}

SharedDictionaryHeaderCheckerSourceStream::
    ~SharedDictionaryHeaderCheckerSourceStream() = default;

int SharedDictionaryHeaderCheckerSourceStream::Read(
    IOBuffer* dest_buffer,
    int buffer_size,
    CompletionOnceCallback callback) {
  if (header_check_result_ == OK) {
    return upstream_->Read(dest_buffer, buffer_size, std::move(callback));
  }
  if (header_check_result_ == ERR_IO_PENDING) {
    CHECK(head_read_buffer_);
    // Still reading header.
    pending_read_buf_ = dest_buffer;
    pending_read_buf_len_ = buffer_size;
    pending_callback_ = std::move(callback);
  }
  return header_check_result_;
}

std::string SharedDictionaryHeaderCheckerSourceStream::Description() const {
  return "SharedDictionaryHeaderCheckerSourceStream";
}

bool SharedDictionaryHeaderCheckerSourceStream::MayHaveMoreBytes() const {
  return upstream_->MayHaveMoreBytes();
}

void SharedDictionaryHeaderCheckerSourceStream::ReadHeader() {
  int result = upstream_->Read(
      head_read_buffer_.get(), head_read_buffer_->RemainingCapacity(),
      base::BindOnce(
          &SharedDictionaryHeaderCheckerSourceStream::OnReadCompleted,
          base::Unretained(this)));
  if (result != ERR_IO_PENDING) {
    OnReadCompleted(result);
  }
}

void SharedDictionaryHeaderCheckerSourceStream::OnReadCompleted(int result) {
  CHECK_NE(result, ERR_IO_PENDING);
  if (result <= 0) {
    // OK means the stream is closed before reading header.
    if (result == OK) {
      result = ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER;
    }
    HeaderCheckCompleted(result);
    return;
  }
  head_read_buffer_->set_offset(head_read_buffer_->offset() + result);
  if (head_read_buffer_->RemainingCapacity() != 0) {
    ReadHeader();
    return;
  }
  HeaderCheckCompleted(
      CheckHeaderBuffer() ? OK : ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER);
}

bool SharedDictionaryHeaderCheckerSourceStream::CheckHeaderBuffer() const {
  CHECK(head_read_buffer_->RemainingCapacity() == 0);
  if (GetSignatureInBuffer() != GetExpectedSignature(type_)) {
    return false;
  }
  if (GetHashInBuffer() != base::span(dictionary_hash_.data)) {
    return false;
  }
  return true;
}

void SharedDictionaryHeaderCheckerSourceStream::HeaderCheckCompleted(
    int header_check_result) {
  CHECK_NE(header_check_result, ERR_IO_PENDING);
  CHECK_EQ(header_check_result_, ERR_IO_PENDING);

  header_check_result_ = header_check_result;
  head_read_buffer_.reset();

  if (!pending_callback_) {
    return;
  }

  auto callback_split = base::SplitOnceCallback(std::move(pending_callback_));
  int read_result = Read(pending_read_buf_.get(), pending_read_buf_len_,
                         std::move(callback_split.first));
  if (read_result != ERR_IO_PENDING) {
    std::move(callback_split.second).Run(read_result);
  }
}

base::span<const unsigned char>
SharedDictionaryHeaderCheckerSourceStream::GetSignatureInBuffer() const {
  return head_read_buffer_->everything().subspan(0, GetSignatureSize(type_));
}

base::span<const unsigned char>
SharedDictionaryHeaderCheckerSourceStream::GetHashInBuffer() const {
  return head_read_buffer_->everything().subspan(
      GetSignatureSize(type_), kCompressionDictionaryHashSize);
}

}  // namespace net
