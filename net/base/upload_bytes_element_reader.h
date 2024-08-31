// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_
#define NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "net/base/net_export.h"
#include "net/base/upload_element_reader.h"

namespace net {

// An UploadElementReader implementation for bytes. The caller owns |bytes|,
// and is responsible for ensuring it outlives the UploadBytesElementReader.
class NET_EXPORT UploadBytesElementReader : public UploadElementReader {
 public:
  explicit UploadBytesElementReader(base::span<const uint8_t> bytes);
  UploadBytesElementReader(const UploadBytesElementReader&) = delete;
  UploadBytesElementReader& operator=(const UploadBytesElementReader&) = delete;
  ~UploadBytesElementReader() override;

  base::span<const uint8_t> bytes() const { return bytes_; }

  // UploadElementReader overrides:
  const UploadBytesElementReader* AsBytesReader() const override;
  int Init(CompletionOnceCallback callback) override;
  uint64_t GetContentLength() const override;
  uint64_t BytesRemaining() const override;
  bool IsInMemory() const override;
  int Read(IOBuffer* buf,
           int buf_length,
           CompletionOnceCallback callback) override;

 private:
  const base::span<const uint8_t> bytes_;
  uint64_t offset_ = 0;
};

// A subclass of UplodBytesElementReader which owns the data given as a vector.
class NET_EXPORT UploadOwnedBytesElementReader
    : public UploadBytesElementReader {
 public:
  // |data| is cleared by this ctor.
  explicit UploadOwnedBytesElementReader(std::vector<char>* data);
  UploadOwnedBytesElementReader(const UploadOwnedBytesElementReader&) = delete;
  UploadOwnedBytesElementReader& operator=(
      const UploadOwnedBytesElementReader&) = delete;
  ~UploadOwnedBytesElementReader() override;

  // Creates UploadOwnedBytesElementReader with a string.
  static std::unique_ptr<UploadOwnedBytesElementReader> CreateWithString(
      const std::string& string);

 private:
  std::vector<char> data_;
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_
