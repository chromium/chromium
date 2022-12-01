// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_reader.h"

#include "base/check_op.h"
#include "base/task/thread_pool.h"
#include "components/web_package/shared_file.h"
#include "content/browser/web_package/web_bundle_blob_data_source.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

WebBundleReader::WebBundleReader(std::unique_ptr<WebBundleSource> source)
    : source_(std::move(source)),
      parser_(std::make_unique<data_decoder::SafeWebBundleParser>(
          /*base_url=*/absl::nullopt)) {
  DCHECK(source_->is_trusted_file() || source_->is_file());
}

WebBundleReader::WebBundleReader(
    std::unique_ptr<WebBundleSource> source,
    int64_t content_length,
    mojo::ScopedDataPipeConsumerHandle outer_response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter)
    : source_(std::move(source)),
      parser_(std::make_unique<data_decoder::SafeWebBundleParser>(
          /*base_url=*/absl::nullopt)) {
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

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](std::unique_ptr<WebBundleSource> source)
                -> std::unique_ptr<base::File> { return source->OpenFile(); },
            source_->Clone()),
        base::BindOnce(&WebBundleReader::OnFileOpened, this,
                       std::move(callback)));
    return;
  }
  DCHECK(source_->is_network());
  parser_->ParseMetadata(
      /*offset=*/-1,
      base::BindOnce(&WebBundleReader::OnMetadataParsed, base::Unretained(this),
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
  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>(
      /*base_url=*/absl::nullopt);

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
        file_->CreateDataSource(response->payload_offset,
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

std::vector<GURL> WebBundleReader::GetEntries() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitial);

  std::vector<GURL> entries;
  entries.reserve(entries_.size());
  base::ranges::transform(entries_, std::back_inserter(entries),
                          [](const auto& entry) { return entry.first; });
  return entries;
}

const absl::optional<GURL>& WebBundleReader::GetPrimaryURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitial);

  return primary_url_;
}

const WebBundleSource& WebBundleReader::source() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *source_;
}

void WebBundleReader::OnFileOpened(MetadataCallback callback,
                                   std::unique_ptr<base::File> file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_->is_trusted_file() || source_->is_file());
  if (!file->IsValid()) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            web_package::mojom::BundleMetadataParseError::New(
                web_package::mojom::BundleParseErrorType::kParserInternalError,
                base::File::ErrorToString(file->error_details()))));
    return;
  }
  file_ = base::MakeRefCounted<web_package::SharedFile>(std::move(file));
  file_->DuplicateFile(base::BindOnce(&WebBundleReader::OnFileDuplicated, this,
                                      std::move(callback)));
}

void WebBundleReader::OnFileDuplicated(MetadataCallback callback,
                                       base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    parser_->ParseMetadata(
        /*offset=*/-1,
        base::BindOnce(&WebBundleReader::OnMetadataParsed,
                       base::Unretained(this), std::move(callback)));
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
    DCHECK(!primary_url_.has_value() || primary_url_->is_valid());
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
