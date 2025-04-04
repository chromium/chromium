// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader_util.h"

#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "services/network/chunked_data_pipe_upload_data_stream.h"
#include "services/network/data_pipe_element_reader.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/url_request.mojom-shared.h"

namespace network {

namespace {

// A subclass of net::UploadBytesElementReader which owns
// ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const DataElementBytes& element)
      : net::UploadBytesElementReader(element.bytes()),
        resource_request_body_(resource_request_body) {}

  BytesElementReader(const BytesElementReader&) = delete;
  BytesElementReader& operator=(const BytesElementReader&) = delete;

  ~BytesElementReader() override = default;

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;
};

// A subclass of net::UploadFileElementReader which owns
// ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const DataElementFile& element,
                    base::File&& file)
      : net::UploadFileElementReader(task_runner,
                                     std::move(file),
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {}

  FileElementReader(const FileElementReader&) = delete;
  FileElementReader& operator=(const FileElementReader&) = delete;

  ~FileElementReader() override = default;

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;
};

}  // namespace
std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    std::vector<base::File>& opened_files,
    base::SequencedTaskRunner* file_task_runner) {
  // In the case of a chunked upload, there will just be one element.
  if (body->elements()->size() == 1) {
    if (body->elements()->begin()->type() ==
        network::mojom::DataElementDataView::Tag::kChunkedDataPipe) {
      auto& element =
          body->elements_mutable()->at(0).As<DataElementChunkedDataPipe>();
      const bool has_null_source = element.read_only_once().value();
      auto upload_data_stream =
          std::make_unique<ChunkedDataPipeUploadDataStream>(
              body, element.ReleaseChunkedDataPipeGetter(), has_null_source);
      if (element.read_only_once()) {
        upload_data_stream->EnableCache();
      }
      return upload_data_stream;
    }
  }

  auto opened_file = opened_files.begin();
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  for (const auto& element : *body->elements()) {
    switch (element.type()) {
      case network::mojom::DataElementDataView::Tag::kBytes:
        element_readers.push_back(std::make_unique<BytesElementReader>(
            body, element.As<DataElementBytes>()));
        break;
      case network::mojom::DataElementDataView::Tag::kFile:
        CHECK(opened_file != opened_files.end(), base::NotFatalUntil::M130);
        element_readers.push_back(std::make_unique<FileElementReader>(
            body, file_task_runner, element.As<network::DataElementFile>(),
            std::move(*opened_file++)));
        break;
      case network::mojom::DataElementDataView::Tag::kDataPipe: {
        element_readers.push_back(std::make_unique<DataPipeElementReader>(
            body,
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter()));
        break;
      }
      case network::mojom::DataElementDataView::Tag::kChunkedDataPipe: {
        // This shouldn't happen, as the traits logic should ensure that if
        // there's a chunked pipe, there's one and only one element.
        NOTREACHED();
      }
    }
  }
  DCHECK(opened_file == opened_files.end());

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), body->identifier());
}

}  // namespace network
