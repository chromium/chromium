// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SHARED_DICTIONARY_SHARED_DICTIONARY_HEADER_CHECKER_SOURCE_STREAM_H_
#define NET_SHARED_DICTIONARY_SHARED_DICTIONARY_HEADER_CHECKER_SOURCE_STREAM_H_

#include <memory>
#include <string>

#include "net/base/completion_once_callback.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/filter/source_stream.h"

namespace net {
class IOBuffer;
class GrowableIOBuffer;

// This class is used to check the header bytes of "Dictionary-Compressed
// Brotli" stream and "Dictionary-Compressed Zstandard" stream.
//
// The "Dictionary-Compressed Brotli" stream's header is 36 bytes containing:
//  - 4 bytes magic number: 0xff, 0x44, 0x43, 0x42
//  - 32 bytes SHA-256 hash digest of the dictionary.
// The "Dictionary-Compressed Zstandard" stream's header is 40 bytes containing:
//  - 8 bytes magic number: 0x5e, 0x2a, 0x4d, 0x18, 0x20, 0x00, 0x00, 0x00
//  - 32 bytes SHA-256 hash digest of the dictionary.
//
// When an error occurred while reading the upstream, this class passes the
// error to the reader of this class. When the header bytes from the upstream
// was different from the expected header, this class passes
// ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER to the reader.
//
// This class consumes the header bytes from the upstream. So the reader of
// this class can read the bytes after the header bytes.
class NET_EXPORT SharedDictionaryHeaderCheckerSourceStream
    : public SourceStream {
 public:
  enum class Type {
    kDictionaryCompressedBrotli,
    kDictionaryCompressedZstd,
  };
  SharedDictionaryHeaderCheckerSourceStream(
      std::unique_ptr<SourceStream> upstream,
      Type type,
      const SHA256HashValue& dictionary_hash);
  SharedDictionaryHeaderCheckerSourceStream(
      const SharedDictionaryHeaderCheckerSourceStream&) = delete;
  SharedDictionaryHeaderCheckerSourceStream& operator=(
      const SharedDictionaryHeaderCheckerSourceStream&) = delete;
  ~SharedDictionaryHeaderCheckerSourceStream() override;

  // SourceStream implementation:
  int Read(IOBuffer* dest_buffer,
           int buffer_size,
           CompletionOnceCallback callback) override;
  std::string Description() const override;
  bool MayHaveMoreBytes() const override;

 private:
  void ReadHeader();
  void OnReadCompleted(int result);
  bool CheckHeaderBuffer() const;
  void HeaderCheckCompleted(int header_check_result);

  base::span<const unsigned char> GetSignatureInBuffer() const;
  base::span<const unsigned char> GetHashInBuffer() const;

  std::unique_ptr<SourceStream> upstream_;
  const Type type_;
  const SHA256HashValue dictionary_hash_;

  scoped_refptr<GrowableIOBuffer> head_read_buffer_;
  int header_check_result_ = ERR_IO_PENDING;

  scoped_refptr<IOBuffer> pending_read_buf_;
  int pending_read_buf_len_ = 0;
  CompletionOnceCallback pending_callback_;
};

}  // namespace net

#endif  // NET_SHARED_DICTIONARY_SHARED_DICTIONARY_HEADER_CHECKER_SOURCE_STREAM_H_
