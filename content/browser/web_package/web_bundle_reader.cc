// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_reader.h"

#include <limits>

#include "base/check_op.h"
#include "base/numerics/safe_math.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/browser/web_package/web_bundle_blob_data_source.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/web_package/web_package_request_matcher.h"

namespace content {

WebBundleReader::SharedFile::SharedFile(
    std::unique_ptr<WebBundleSource> source) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](std::unique_ptr<WebBundleSource> source)
              -> std::unique_ptr<base::File> { return source->OpenFile(); },
          std::move(source)),
      base::BindOnce(&SharedFile::SetFile, base::RetainedRef(this)));
}

void WebBundleReader::SharedFile::DuplicateFile(
    base::OnceCallback<void(base::File)> callback) {
  // Basically this interface expects this method is called at most once. Have
  // a DCHECK for the case that does not work for a clear reason, just in case.
  // The call site also have another DCHECK for external callers not to cause
  // such problematic cases.
  DCHECK(duplicate_callback_.is_null());
  duplicate_callback_ = std::move(callback);

  if (file_)
    SetFile(std::move(file_));
}

base::File* WebBundleReader::SharedFile::operator->() {
  DCHECK(file_);
  return file_.get();
}

WebBundleReader::SharedFile::~SharedFile() {
  // Move the last reference to |file_| that leads an internal blocking call
  // that is not permitted here.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce([](std::unique_ptr<base::File> file) {},
                     std::move(file_)));
}

void WebBundleReader::SharedFile::SetFile(std::unique_ptr<base::File> file) {
  file_ = std::move(file);

  if (duplicate_callback_.is_null())
    return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](base::File* file) -> base::File { return file->Duplicate(); },
          file_.get()),
      std::move(duplicate_callback_));
}

class WebBundleReader::SharedFileDataSource final
    : public mojo::DataPipeProducer::DataSource {
 public:
  SharedFileDataSource(scoped_refptr<WebBundleReader::SharedFile> file,
                       uint64_t offset,
                       uint64_t length)
      : file_(std::move(file)), offset_(offset), length_(length) {
    error_ = mojo::FileDataSource::ConvertFileErrorToMojoResult(
        (*file_)->error_details());

    // base::File::Read takes int64_t as an offset. So, offset + length should
    // not overflow in int64_t.
    uint64_t max_offset;
    if (!base::CheckAdd(offset, length).AssignIfValid(&max_offset) ||
        (std::numeric_limits<int64_t>::max() < max_offset)) {
      error_ = MOJO_RESULT_INVALID_ARGUMENT;
    }
  }

  SharedFileDataSource(const SharedFileDataSource&) = delete;
  SharedFileDataSource& operator=(const SharedFileDataSource&) = delete;

 private:
  // Implements mojo::DataPipeProducer::DataSource. Following methods are called
  // on a blockable sequenced task runner.
  uint64_t GetLength() const override { return length_; }
  ReadResult Read(uint64_t offset, base::span<char> buffer) override {
    ReadResult result;
    result.result = error_;

    if (length_ < offset)
      result.result = MOJO_RESULT_INVALID_ARGUMENT;

    if (result.result != MOJO_RESULT_OK)
      return result;

    uint64_t readable_size = length_ - offset;
    uint64_t writable_size = buffer.size();
    uint64_t copyable_size =
        std::min(std::min(readable_size, writable_size),
                 static_cast<uint64_t>(std::numeric_limits<int>::max()));

    int bytes_read =
        (*file_)->Read(offset_ + offset, buffer.data(), copyable_size);
    if (bytes_read < 0) {
      result.result = mojo::FileDataSource::ConvertFileErrorToMojoResult(
          (*file_)->GetLastFileError());
    } else {
      result.bytes_read = bytes_read;
    }
    return result;
  }

  scoped_refptr<WebBundleReader::SharedFile> file_;
  MojoResult error_;
  const uint64_t offset_;
  const uint64_t length_;
};

WebBundleReader::WebBundleReader(std::unique_ptr<WebBundleSource> source)
    : source_(std::move(source)),
      parser_(std::make_unique<data_decoder::SafeWebBundleParser>()),
      file_(base::MakeRefCounted<SharedFile>(source_->Clone())) {
  DCHECK(source_->is_trusted_file() || source_->is_file());
}

WebBundleReader::WebBundleReader(
    std::unique_ptr<WebBundleSource> source,
    int64_t content_length,
    mojo::ScopedDataPipeConsumerHandle outer_response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter)
    : source_(std::move(source)),
      parser_(std::make_unique<data_decoder::SafeWebBundleParser>()) {
  DCHECK(source_->is_network());
  mojo::PendingRemote<web_package::mojom::BundleDataSource> pending_remote;
  blob_data_source_ = std::make_unique<WebBundleBlobDataSource>(
      content_length, std::move(outer_response_body), std::move(endpoints),
      std::move(blob_context_getter));
  blob_data_source_->AddReceiver(
      pending_remote.InitWithNewPipeAndPassReceiver());
  parser_->OpenDataSource(std::move(pending_remote));
}

WebBundleReader::~WebBundleReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebBundleReader::ReadMetadata(MetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kInitial);

  if (!blob_data_source_) {
    DCHECK(source_->is_trusted_file() || source_->is_file());
    file_->DuplicateFile(base::BindOnce(&WebBundleReader::ReadMetadataInternal,
                                        this, std::move(callback)));
    return;
  }
  DCHECK(source_->is_network());
  parser_->ParseMetadata(base::BindOnce(&WebBundleReader::OnMetadataParsed,
                                        base::Unretained(this),
                                        std::move(callback)));
}

void WebBundleReader::ReadResponse(
    const network::ResourceRequest& resource_request,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitial);

  auto it = entries_.find(net::SimplifyUrlForRequest(resource_request.url));
  if (it == entries_.end()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), nullptr,
            web_package::mojom::BundleResponseParseError::New(
                web_package::mojom::BundleParseErrorType::kParserInternalError,
                "Not found in Web Bundle file.")));
    return;
  }
  auto response_location = it->second.Clone();

  if (state_ == State::kDisconnected) {
    // Try reconnecting, if not attempted yet.
    if (pending_read_responses_.empty())
      Reconnect();
    pending_read_responses_.emplace_back(std::move(response_location),
                                         std::move(callback));
    return;
  }

  ReadResponseInternal(std::move(response_location), std::move(callback));
}

void WebBundleReader::ReadResponseInternal(
    web_package::mojom::BundleResponseLocationPtr location,
    ResponseCallback callback) {
  parser_->ParseResponse(
      location->offset, location->length,
      base::BindOnce(&WebBundleReader::OnResponseParsed, base::Unretained(this),
                     std::move(callback)));
}

void WebBundleReader::Reconnect() {
  DCHECK(!parser_);
  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>();

  if (!blob_data_source_) {
    DCHECK(source_->is_trusted_file() || source_->is_file());
    file_->DuplicateFile(
        base::BindOnce(&WebBundleReader::ReconnectForFile, this));
    return;
  }
  DCHECK(source_->is_network());
  mojo::PendingRemote<web_package::mojom::BundleDataSource> pending_remote;
  blob_data_source_->AddReceiver(
      pending_remote.InitWithNewPipeAndPassReceiver());
  parser_->OpenDataSource(std::move(pending_remote));

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&WebBundleReader::DidReconnect, this,
                                absl::nullopt /* error */));
}

void WebBundleReader::ReconnectForFile(base::File file) {
  base::File::Error file_error = parser_->OpenFile(std::move(file));
  absl::optional<std::string> error;
  if (file_error != base::File::FILE_OK)
    error = base::File::ErrorToString(file_error);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebBundleReader::DidReconnect, this, std::move(error)));
}

void WebBundleReader::DidReconnect(absl::optional<std::string> error) {
  DCHECK_EQ(state_, State::kDisconnected);
  DCHECK(parser_);
  auto read_tasks = std::move(pending_read_responses_);

  if (error) {
    for (auto& pair : read_tasks) {
      base::ThreadPool::PostTask(
          FROM_HERE,
          base::BindOnce(std::move(pair.second), nullptr,
                         web_package::mojom::BundleResponseParseError::New(
                             web_package::mojom::BundleParseErrorType::
                                 kParserInternalError,
                             *error)));
    }
    return;
  }

  state_ = State::kMetadataReady;
  parser_->SetDisconnectCallback(base::BindOnce(
      &WebBundleReader::OnParserDisconnected, base::Unretained(this)));
  for (auto& pair : read_tasks)
    ReadResponseInternal(std::move(pair.first), std::move(pair.second));
}

void WebBundleReader::ReadResponseBody(
    web_package::mojom::BundleResponsePtr response,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    BodyCompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitial);

  if (!blob_data_source_) {
    DCHECK(source_->is_trusted_file() || source_->is_file());
    auto data_producer =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    mojo::DataPipeProducer* raw_producer = data_producer.get();
    raw_producer->Write(
        std::make_unique<SharedFileDataSource>(file_, response->payload_offset,
                                               response->payload_length),
        base::BindOnce(
            [](std::unique_ptr<mojo::DataPipeProducer> producer,
               BodyCompletionCallback callback, MojoResult result) {
              std::move(callback).Run(
                  result == MOJO_RESULT_OK ? net::OK : net::ERR_UNEXPECTED);
            },
            std::move(data_producer), std::move(callback)));
    return;
  }

  DCHECK(source_->is_network());
  blob_data_source_->ReadToDataPipe(
      response->payload_offset, response->payload_length,
      std::move(producer_handle), std::move(callback));
}

bool WebBundleReader::HasEntry(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitial);

  return entries_.contains(net::SimplifyUrlForRequest(url));
}

const GURL& WebBundleReader::GetPrimaryURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitial);

  return primary_url_;
}

const WebBundleSource& WebBundleReader::source() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *source_;
}

void WebBundleReader::ReadMetadataInternal(MetadataCallback callback,
                                           base::File file) {
  DCHECK(source_->is_trusted_file() || source_->is_file());
  base::File::Error error = parser_->OpenFile(std::move(file));
  if (base::File::FILE_OK != error) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            web_package::mojom::BundleMetadataParseError::New(
                web_package::mojom::BundleParseErrorType::kParserInternalError,
                base::File::ErrorToString(error))));
  } else {
    parser_->ParseMetadata(base::BindOnce(&WebBundleReader::OnMetadataParsed,
                                          base::Unretained(this),
                                          std::move(callback)));
  }
}

void WebBundleReader::OnMetadataParsed(
    MetadataCallback callback,
    web_package::mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kInitial);

  state_ = State::kMetadataReady;
  parser_->SetDisconnectCallback(base::BindOnce(
      &WebBundleReader::OnParserDisconnected, base::Unretained(this)));

  if (metadata) {
    primary_url_ = metadata->primary_url;
    entries_ = std::move(metadata->requests);
  }
  std::move(callback).Run(std::move(error));
}

void WebBundleReader::OnResponseParsed(
    ResponseCallback callback,
    web_package::mojom::BundleResponsePtr response,
    web_package::mojom::BundleResponseParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitial);

  std::move(callback).Run(std::move(response), std::move(error));
}

void WebBundleReader::OnParserDisconnected() {
  DCHECK_EQ(state_, State::kMetadataReady);

  state_ = State::kDisconnected;
  parser_ = nullptr;
  // Reconnection will be attempted on next ReadResponse() call.
}

}  // namespace content
