// Copyright 2013 The Chromium Authors
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

DataElementBytes::DataElementBytes() = default;
DataElementBytes::DataElementBytes(std::vector<uint8_t> bytes)
    : bytes_(std::move(bytes)) {}
DataElementBytes::DataElementBytes(DataElementBytes&& other) = default;
DataElementBytes& DataElementBytes::operator=(DataElementBytes&& other) =
    default;
DataElementBytes::~DataElementBytes() = default;

DataElementBytes DataElementBytes::Clone() const {
  return DataElementBytes(bytes_);
}

DataElementDataPipe::DataElementDataPipe() = default;
DataElementDataPipe::DataElementDataPipe(
    mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter)
    : data_pipe_getter_(std::move(data_pipe_getter)) {
  DCHECK(data_pipe_getter_);
}
DataElementDataPipe::DataElementDataPipe(DataElementDataPipe&&) = default;
DataElementDataPipe& DataElementDataPipe::operator=(
    DataElementDataPipe&& other) = default;
DataElementDataPipe::~DataElementDataPipe() = default;

mojo::PendingRemote<mojom::DataPipeGetter>
DataElementDataPipe::ReleaseDataPipeGetter() {
  DCHECK(data_pipe_getter_.is_valid());
  return std::move(data_pipe_getter_);
}

mojo::PendingRemote<mojom::DataPipeGetter>
DataElementDataPipe::CloneDataPipeGetter() const {
  DCHECK(data_pipe_getter_.is_valid());
  auto* mutable_this = const_cast<DataElementDataPipe*>(this);
  mojo::Remote<mojom::DataPipeGetter> owned(
      std::move(mutable_this->data_pipe_getter_));
  mojo::PendingRemote<mojom::DataPipeGetter> clone;
  owned->Clone(clone.InitWithNewPipeAndPassReceiver());
  mutable_this->data_pipe_getter_ = owned.Unbind();
  return clone;
}

DataElementDataPipe DataElementDataPipe::Clone() const {
  return DataElementDataPipe(CloneDataPipeGetter());
}

DataElementChunkedDataPipe::DataElementChunkedDataPipe() = default;
DataElementChunkedDataPipe::DataElementChunkedDataPipe(
    mojo::PendingRemote<mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter,
    ReadOnlyOnce read_only_once)
    : chunked_data_pipe_getter_(std::move(chunked_data_pipe_getter)),
      read_only_once_(read_only_once) {
  DCHECK(chunked_data_pipe_getter_);
}
DataElementChunkedDataPipe::DataElementChunkedDataPipe(
    DataElementChunkedDataPipe&& other) = default;
DataElementChunkedDataPipe& DataElementChunkedDataPipe::operator=(
    DataElementChunkedDataPipe&& other) = default;
DataElementChunkedDataPipe::~DataElementChunkedDataPipe() = default;

mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
DataElementChunkedDataPipe::ReleaseChunkedDataPipeGetter() {
  DCHECK(chunked_data_pipe_getter_.is_valid());
  return std::move(chunked_data_pipe_getter_);
}

DataElementFile::DataElementFile() = default;
DataElementFile::DataElementFile(const base::FilePath& path,
                                 uint64_t offset,
                                 uint64_t length,
                                 base::Time expected_modification_time)
    : path_(path),
      offset_(offset),
      length_(length),
      expected_modification_time_(expected_modification_time) {}
DataElementFile::DataElementFile(const DataElementFile&) = default;
DataElementFile& DataElementFile::operator=(const DataElementFile&) = default;
DataElementFile::DataElementFile(DataElementFile&&) = default;
DataElementFile& DataElementFile::operator=(DataElementFile&&) = default;
DataElementFile::~DataElementFile() = default;

DataElement::DataElement() = default;
DataElement::DataElement(DataElement&& other) = default;
DataElement& DataElement::operator=(DataElement&& other) = default;
DataElement::~DataElement() = default;

DataElement DataElement::Clone() const {
  switch (type()) {
    case Tag::kBytes:
      return DataElement(As<DataElementBytes>().Clone());
    case network::DataElement::Tag::kDataPipe:
      return DataElement(As<DataElementDataPipe>().Clone());
    case network::DataElement::Tag::kFile:
      return DataElement(As<DataElementFile>());
    case network::DataElement::Tag::kChunkedDataPipe:
      // DataElementChunkedDataPipe is not generally copyable, especially if
      // `read_only_once` is true.
      // We want to be strict on this case, and use CHECK rather than DCHECK.
      CHECK(false);
      return DataElement();
  }
}

}  // namespace network
