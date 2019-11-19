// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_loader.h"

#include <stddef.h>
#include <utility>
#include "base/bind.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace storage {

namespace {

scoped_refptr<net::HttpResponseHeaders> GenerateHeaders(
    net::HttpStatusCode status_code,
    BlobDataHandle* blob_handle,
    net::HttpByteRange* byte_range,
    uint64_t total_size,
    uint64_t content_size) {
  std::string status("HTTP/1.1 ");
  status.append(base::NumberToString(status_code));
  status.append(" ");
  status.append(net::GetHttpReasonPhrase(status_code));
  status.append("\0\0", 2);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(status);

  if (status_code == net::HTTP_OK || status_code == net::HTTP_PARTIAL_CONTENT) {
    std::string content_length_header(net::HttpRequestHeaders::kContentLength);
    content_length_header.append(": ");
    content_length_header.append(base::NumberToString(content_size));
    headers->AddHeader(content_length_header);
    if (status_code == net::HTTP_PARTIAL_CONTENT) {
      DCHECK(byte_range->IsValid());
      std::string content_range_header(net::HttpResponseHeaders::kContentRange);
      content_range_header.append(": bytes ");
      content_range_header.append(base::StringPrintf(
          "%" PRId64 "-%" PRId64, byte_range->first_byte_position(),
          byte_range->last_byte_position()));
      content_range_header.append("/");
      content_range_header.append(base::StringPrintf("%" PRId64, total_size));
      headers->AddHeader(content_range_header);
    }
    if (!blob_handle->content_type().empty()) {
      std::string content_type_header(net::HttpRequestHeaders::kContentType);
      content_type_header.append(": ");
      content_type_header.append(blob_handle->content_type());
      headers->AddHeader(content_type_header);
    }
    if (!blob_handle->content_disposition().empty()) {
      std::string content_disposition_header("Content-Disposition: ");
      content_disposition_header.append(blob_handle->content_disposition());
      headers->AddHeader(content_disposition_header);
    }
  }

  return headers;
}

}  // namespace

// static
void BlobURLLoader::CreateAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    std::unique_ptr<BlobDataHandle> blob_handle) {
  new BlobURLLoader(std::move(url_loader_receiver), request, std::move(client),
                    std::move(blob_handle));
}

BlobURLLoader::~BlobURLLoader() = default;

BlobURLLoader::BlobURLLoader(
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    std::unique_ptr<BlobDataHandle> blob_handle)
    : receiver_(this, std::move(url_loader_receiver)),
      client_(std::move(client)),
      blob_handle_(std::move(blob_handle)) {
  // PostTask since it might destruct.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BlobURLLoader::Start,
                                weak_factory_.GetWeakPtr(), request));
}

void BlobURLLoader::Start(const network::ResourceRequest& request) {
  if (!blob_handle_) {
    OnComplete(net::ERR_FILE_NOT_FOUND, 0);
    delete this;
    return;
  }

  // We only support GET request per the spec.
  if (request.method != "GET") {
    OnComplete(net::ERR_METHOD_NOT_SUPPORTED, 0);
    delete this;
    return;
  }

  std::string range_header;
  if (request.headers.GetHeader(net::HttpRequestHeaders::kRange,
                                &range_header)) {
    // We only care about "Range" header here.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_set_ = true;
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request,
        // because we need to do multipart encoding here.
        // TODO(jianli): Support multipart byte range requests.
        OnComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, 0);
        delete this;
        return;
      }
    }
  }
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = network::kDataPipeDefaultAllocationSize;
  if (mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle) !=
      MOJO_RESULT_OK) {
    OnComplete(net::ERR_INSUFFICIENT_RESOURCES, 0);
    delete this;
    return;
  }
  response_body_consumer_handle_ = std::move(consumer_handle);

  MojoBlobReader::Create(blob_handle_.get(), byte_range_,
                         base::WrapUnique(this), std::move(producer_handle));
}

void BlobURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  NOTREACHED();
}

MojoBlobReader::Delegate::RequestSideData BlobURLLoader::DidCalculateSize(
    uint64_t total_size,
    uint64_t content_size) {
  total_size_ = total_size;
  bool result = byte_range_.ComputeBounds(total_size);
  DCHECK(result);

  net::HttpStatusCode status_code = net::HTTP_OK;
  if (byte_range_set_ && byte_range_.IsValid()) {
    status_code = net::HTTP_PARTIAL_CONTENT;
  } else {
    DCHECK_EQ(total_size, content_size);
    // TODO(horo): When the requester doesn't need the side data
    // (ex:FileReader) we should skip reading the side data.
    return REQUEST_SIDE_DATA;
  }

  HeadersCompleted(status_code, content_size, base::nullopt);
  return DONT_REQUEST_SIDE_DATA;
}

void BlobURLLoader::DidReadSideData(base::Optional<mojo_base::BigBuffer> data) {
  HeadersCompleted(net::HTTP_OK, total_size_, std::move(data));
}

void BlobURLLoader::OnComplete(net::Error error_code,
                               uint64_t total_written_bytes) {
  base::UmaHistogramSparse("Storage.Blob.BlobUrlLoader.FailureType",
                           error_code);

  network::URLLoaderCompletionStatus status(error_code);
  status.encoded_body_length = total_written_bytes;
  status.decoded_body_length = total_written_bytes;
  client_->OnComplete(status);
}
void BlobURLLoader::HeadersCompleted(
    net::HttpStatusCode status_code,
    uint64_t content_size,
    base::Optional<mojo_base::BigBuffer> metadata) {
  auto response = network::mojom::URLResponseHead::New();
  response->content_length = 0;
  if (status_code == net::HTTP_OK || status_code == net::HTTP_PARTIAL_CONTENT)
    response->content_length = content_size;
  response->headers = GenerateHeaders(status_code, blob_handle_.get(),
                                      &byte_range_, total_size_, content_size);

  std::string mime_type;
  response->headers->GetMimeType(&mime_type);
  // Match logic in StreamURLRequestJob::HeadersCompleted.
  if (mime_type.empty())
    mime_type = "text/plain";
  response->mime_type = mime_type;

  // TODO(jam): some of this code can be shared with
  // services/network/url_loader.h
  client_->OnReceiveResponse(std::move(response));
  sent_headers_ = true;

  if (metadata.has_value())
    client_->OnReceiveCachedMetadata(std::move(metadata.value()));

  client_->OnStartLoadingResponseBody(
      std::move(response_body_consumer_handle_));
}

}  // namespace storage
