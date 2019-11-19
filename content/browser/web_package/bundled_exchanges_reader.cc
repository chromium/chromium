// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_reader.h"

#include <limits>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/browser/web_package/bundled_exchanges_blob_data_source.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/base/url_util.h"

namespace content {

BundledExchangesReader::SharedFile::SharedFile(
    std::unique_ptr<BundledExchangesSource> source) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(
          [](std::unique_ptr<BundledExchangesSource> source)
              -> std::unique_ptr<base::File> { return source->OpenFile(); },
          std::move(source)),
      base::BindOnce(&SharedFile::SetFile, base::RetainedRef(this)));
}

void BundledExchangesReader::SharedFile::DuplicateFile(
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

base::File* BundledExchangesReader::SharedFile::operator->() {
  DCHECK(file_);
  return file_.get();
}

BundledExchangesReader::SharedFile::~SharedFile() {
  // Move the last reference to |file_| that leads an internal blocking call
  // that is not permitted here.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce([](std::unique_ptr<base::File> file) {},
                     std::move(file_)));
}

void BundledExchangesReader::SharedFile::SetFile(
    std::unique_ptr<base::File> file) {
  file_ = std::move(file);

  if (duplicate_callback_.is_null())
    return;

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(
          [](base::File* file) -> base::File { return file->Duplicate(); },
          file_.get()),
      std::move(duplicate_callback_));
}

class BundledExchangesReader::SharedFileDataSource final
    : public mojo::DataPipeProducer::DataSource {
 public:
  SharedFileDataSource(scoped_refptr<BundledExchangesReader::SharedFile> file,
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

  scoped_refptr<BundledExchangesReader::SharedFile> file_;
  MojoResult error_;
  const uint64_t offset_;
  const uint64_t length_;

  DISALLOW_COPY_AND_ASSIGN(SharedFileDataSource);
};

BundledExchangesReader::BundledExchangesReader(
    std::unique_ptr<BundledExchangesSource> source)
    : source_(std::move(source)),
      file_(base::MakeRefCounted<SharedFile>(source_->Clone())) {
  DCHECK(source_->is_trusted_file() || source_->is_file());
}

BundledExchangesReader::BundledExchangesReader(
    std::unique_ptr<BundledExchangesSource> source,
    int64_t content_length,
    mojo::ScopedDataPipeConsumerHandle outer_response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter)
    : source_(std::move(source)) {
  DCHECK(source_->is_network());
  mojo::PendingRemote<data_decoder::mojom::BundleDataSource> pending_remote;
  blob_data_source_ = std::make_unique<BundledExchangesBlobDataSource>(
      content_length, std::move(outer_response_body), std::move(endpoints),
      std::move(blob_context_getter),
      pending_remote.InitWithNewPipeAndPassReceiver());
  parser_.OpenDataSource(std::move(pending_remote));
}

BundledExchangesReader::~BundledExchangesReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BundledExchangesReader::ReadMetadata(MetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!metadata_ready_);
  if (!blob_data_source_) {
    DCHECK(source_->is_trusted_file() || source_->is_file());
    file_->DuplicateFile(
        base::BindOnce(&BundledExchangesReader::ReadMetadataInternal, this,
                       std::move(callback)));
    return;
  }
  DCHECK(source_->is_network());
  parser_.ParseMetadata(
      base::BindOnce(&BundledExchangesReader::OnMetadataParsed,
                     base::Unretained(this), std::move(callback)));
}

void BundledExchangesReader::ReadResponse(const GURL& url,
                                          ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  auto it = entries_.find(net::SimplifyUrlForRequest(url));
  if (it == entries_.end() || it->second->response_locations.empty()) {
    PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), nullptr,
            data_decoder::mojom::BundleResponseParseError::New(
                data_decoder::mojom::BundleParseErrorType::kParserInternalError,
                "Not found in Web Bundle file.")));
    return;
  }

  // For now, this always reads the first response in |response_locations|.
  // TODO(crbug.com/966753): This method should take request headers and choose
  // the most suitable response based on the variant matching algorithm
  // (https://tools.ietf.org/html/draft-ietf-httpbis-variants-05#section-4).
  parser_.ParseResponse(
      it->second->response_locations[0]->offset,
      it->second->response_locations[0]->length,
      base::BindOnce(&BundledExchangesReader::OnResponseParsed,
                     base::Unretained(this), std::move(callback)));
}

void BundledExchangesReader::ReadResponseBody(
    data_decoder::mojom::BundleResponsePtr response,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    BodyCompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

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

bool BundledExchangesReader::HasEntry(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  return entries_.contains(net::SimplifyUrlForRequest(url));
}

const GURL& BundledExchangesReader::GetPrimaryURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  return primary_url_;
}

const BundledExchangesSource& BundledExchangesReader::source() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *source_;
}

void BundledExchangesReader::SetBundledExchangesParserFactoryForTesting(
    mojo::Remote<data_decoder::mojom::BundledExchangesParserFactory> factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  parser_.SetBundledExchangesParserFactoryForTesting(std::move(factory));
}

void BundledExchangesReader::ReadMetadataInternal(MetadataCallback callback,
                                                  base::File file) {
  DCHECK(source_->is_trusted_file() || source_->is_file());
  base::File::Error error = parser_.OpenFile(std::move(file));
  if (base::File::FILE_OK != error) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            std::move(callback),
            data_decoder::mojom::BundleMetadataParseError::New(
                data_decoder::mojom::BundleParseErrorType::kParserInternalError,
                GURL() /* fallback_url */, base::File::ErrorToString(error))));
  } else {
    parser_.ParseMetadata(
        base::BindOnce(&BundledExchangesReader::OnMetadataParsed,
                       base::Unretained(this), std::move(callback)));
  }
}

void BundledExchangesReader::OnMetadataParsed(
    MetadataCallback callback,
    data_decoder::mojom::BundleMetadataPtr metadata,
    data_decoder::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!metadata_ready_);

  metadata_ready_ = true;

  if (metadata) {
    primary_url_ = metadata->primary_url;
    entries_ = std::move(metadata->requests);
  }
  std::move(callback).Run(std::move(error));
}

void BundledExchangesReader::OnResponseParsed(
    ResponseCallback callback,
    data_decoder::mojom::BundleResponsePtr response,
    data_decoder::mojom::BundleResponseParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  std::move(callback).Run(std::move(response), std::move(error));
}

}  // namespace content
