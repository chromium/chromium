// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/public/cpp/resource_request_body.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_request.mojom-shared.h"

namespace network {

ResourceRequestBody::ResourceRequestBody()
    : identifier_(0), contains_sensitive_info_(false) {}

// static
scoped_refptr<ResourceRequestBody> ResourceRequestBody::CreateFromBytes(
    const char* bytes,
    size_t length) {
  scoped_refptr<ResourceRequestBody> result = new ResourceRequestBody();
  result->AppendBytes(bytes, length);
  return result;
}

bool ResourceRequestBody::EnableToAppendElement() const {
  return elements_.empty() ||
         (elements_.front().type() !=
          mojom::DataElementDataView::Tag::kChunkedDataPipe);
}

void ResourceRequestBody::AppendBytes(std::vector<uint8_t> bytes) {
  DCHECK(EnableToAppendElement());

  if (bytes.size() > 0) {
    elements_.emplace_back(DataElementBytes(std::move(bytes)));
  }
}

void ResourceRequestBody::AppendBytes(const char* bytes, int bytes_len) {
  std::vector<uint8_t> vec;
  vec.assign(reinterpret_cast<const uint8_t*>(bytes),
             reinterpret_cast<const uint8_t*>(bytes + bytes_len));

  AppendBytes(std::move(vec));
}

void ResourceRequestBody::AppendFileRange(
    const base::FilePath& file_path,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  DCHECK(EnableToAppendElement());

  elements_.emplace_back(
      DataElementFile(file_path, offset, length, expected_modification_time));
}

void ResourceRequestBody::AppendDataPipe(
    mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter) {
  DCHECK(EnableToAppendElement());
  DCHECK(data_pipe_getter);

  elements_.emplace_back(DataElementDataPipe(std::move(data_pipe_getter)));
}

void ResourceRequestBody::SetToChunkedDataPipe(
    mojo::PendingRemote<mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter,
    ReadOnlyOnce read_only_once) {
  DCHECK(elements_.empty());
  DCHECK(chunked_data_pipe_getter);

  elements_.emplace_back(DataElementChunkedDataPipe(
      std::move(chunked_data_pipe_getter), read_only_once));
}

std::vector<base::FilePath> ResourceRequestBody::GetReferencedFiles() const {
  std::vector<base::FilePath> result;
  for (const auto& element : *elements()) {
    if (element.type() == mojom::DataElementDataView::Tag::kFile) {
      result.push_back(element.As<DataElementFile>().path());
    }
  }
  return result;
}

ResourceRequestBody::~ResourceRequestBody() {}

}  // namespace network
