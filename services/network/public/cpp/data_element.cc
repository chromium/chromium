// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/data_element.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"

namespace network {

const uint64_t DataElement::kUnknownSize;

DataElement::DataElement()
    : type_(mojom::DataElementType::kUnknown),
      bytes_(NULL),
      offset_(0),
      length_(std::numeric_limits<uint64_t>::max()) {}

DataElement::~DataElement() = default;

DataElement::DataElement(DataElement&& other) = default;
DataElement& DataElement::operator=(DataElement&& other) = default;

void DataElement::SetToFilePathRange(
    const base::FilePath& path,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  type_ = mojom::DataElementType::kFile;
  path_ = path;
  offset_ = offset;
  length_ = length;
  expected_modification_time_ = expected_modification_time;
}

void DataElement::SetToFileRange(base::File file,
                                 const base::FilePath& path,
                                 uint64_t offset,
                                 uint64_t length,
                                 const base::Time& expected_modification_time) {
  type_ = mojom::DataElementType::kRawFile;
  file_ = std::move(file);
  path_ = path;
  offset_ = offset;
  length_ = length;
  expected_modification_time_ = expected_modification_time;
}

void DataElement::SetToBlobRange(const std::string& blob_uuid,
                                 uint64_t offset,
                                 uint64_t length) {
  type_ = mojom::DataElementType::kBlob;
  blob_uuid_ = blob_uuid;
  offset_ = offset;
  length_ = length;
}

void DataElement::SetToDataPipe(
    mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter) {
  DCHECK(data_pipe_getter);
  type_ = mojom::DataElementType::kDataPipe;
  data_pipe_getter_ = std::move(data_pipe_getter);
}

void DataElement::SetToChunkedDataPipe(
    mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
        chunked_data_pipe_getter) {
  type_ = mojom::DataElementType::kChunkedDataPipe;
  chunked_data_pipe_getter_ = std::move(chunked_data_pipe_getter);
}

base::File DataElement::ReleaseFile() {
  return std::move(file_);
}

mojo::PendingRemote<mojom::DataPipeGetter>
DataElement::ReleaseDataPipeGetter() {
  DCHECK_EQ(mojom::DataElementType::kDataPipe, type_);
  DCHECK(data_pipe_getter_.is_valid());
  return std::move(data_pipe_getter_);
}

mojo::PendingRemote<mojom::DataPipeGetter> DataElement::CloneDataPipeGetter()
    const {
  DCHECK_EQ(mojom::DataElementType::kDataPipe, type_);
  DCHECK(data_pipe_getter_.is_valid());
  auto* mutable_this = const_cast<DataElement*>(this);
  mojo::Remote<mojom::DataPipeGetter> owned(
      std::move(mutable_this->data_pipe_getter_));
  mojo::PendingRemote<mojom::DataPipeGetter> clone;
  owned->Clone(clone.InitWithNewPipeAndPassReceiver());
  mutable_this->data_pipe_getter_ = owned.Unbind();
  return clone;
}

mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
DataElement::ReleaseChunkedDataPipeGetter() {
  DCHECK_EQ(mojom::DataElementType::kChunkedDataPipe, type_);
  return std::move(chunked_data_pipe_getter_);
}

void PrintTo(const DataElement& x, std::ostream* os) {
  const uint64_t kMaxDataPrintLength = 40;
  *os << "<DataElement>{type: ";
  switch (x.type()) {
    case mojom::DataElementType::kBytes: {
      uint64_t length = std::min(x.length(), kMaxDataPrintLength);
      *os << "TYPE_BYTES, data: ["
          << base::HexEncode(x.bytes(), static_cast<size_t>(length));
      if (length < x.length()) {
        *os << "<...truncated due to length...>";
      }
      *os << "]";
      break;
    }
    case mojom::DataElementType::kFile:
      *os << "TYPE_FILE, path: " << x.path().AsUTF8Unsafe()
          << ", expected_modification_time: " << x.expected_modification_time();
      break;
    case mojom::DataElementType::kRawFile:
      *os << "TYPE_RAW_FILE, path: " << x.path().AsUTF8Unsafe()
          << ", expected_modification_time: " << x.expected_modification_time();
      break;
    case mojom::DataElementType::kBlob:
      *os << "TYPE_BLOB, uuid: " << x.blob_uuid();
      break;
    case mojom::DataElementType::kDataPipe:
      *os << "TYPE_DATA_PIPE";
      break;
    case mojom::DataElementType::kChunkedDataPipe:
      *os << "TYPE_CHUNKED_DATA_PIPE";
      break;
    case mojom::DataElementType::kUnknown:
      *os << "TYPE_UNKNOWN";
      break;
  }
  *os << ", length: " << x.length() << ", offset: " << x.offset() << "}";
}

bool operator==(const DataElement& a, const DataElement& b) {
  if (a.type() != b.type() || a.offset() != b.offset() ||
      a.length() != b.length())
    return false;
  switch (a.type()) {
    case mojom::DataElementType::kBytes:
      return memcmp(a.bytes(), b.bytes(), b.length()) == 0;
    case mojom::DataElementType::kFile:
      return a.path() == b.path() &&
             a.expected_modification_time() == b.expected_modification_time();
    case mojom::DataElementType::kRawFile:
      return a.path() == b.path() &&
             a.expected_modification_time() == b.expected_modification_time();
    case mojom::DataElementType::kBlob:
      return a.blob_uuid() == b.blob_uuid();
    case mojom::DataElementType::kDataPipe:
      return false;
    case mojom::DataElementType::kChunkedDataPipe:
      return false;
    case mojom::DataElementType::kUnknown:
      NOTREACHED();
      return false;
  }
  return false;
}

bool operator!=(const DataElement& a, const DataElement& b) {
  return !(a == b);
}

}  // namespace network
