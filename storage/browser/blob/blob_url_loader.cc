// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_loader.h"

#include <stddef.h>
#include <utility>
#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/mojo_blob_reader.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

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
      base::MakeRefCounted<net::HttpResponseHeaders>(status);

  if (status_code == net::HTTP_OK || status_code == net::HTTP_PARTIAL_CONTENT) {
    headers->SetHeader(net::HttpRequestHeaders::kContentLength,
                       base::NumberToString(content_size));
    if (status_code == net::HTTP_PARTIAL_CONTENT) {
      DCHECK(byte_range->IsValid());
      std::string content_range_header;
      content_range_header.append("bytes ");
      content_range_header.append(base::StringPrintf(
          "%" PRId64 "-%" PRId64, byte_range->first_byte_position(),
          byte_range->last_byte_position()));
      content_range_header.append("/");
      content_range_header.append(base::StringPrintf("%" PRId64, total_size));
      headers->SetHeader(net::HttpResponseHeaders::kContentRange,
                         content_range_header);
    }
    headers->SetHeader(net::HttpRequestHeaders::kContentType,
                       blob_handle->content_type());
    if (!blob_handle->content_disposition().empty()) {
      headers->SetHeader("Content-Disposition",
                         blob_handle->content_disposition());
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
  new BlobURLLoader(std::move(url_loader_receiver), request.method,
                    request.headers, std::move(client), std::move(blob_handle));
}

// static
void BlobURLLoader::CreateAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    std::unique_ptr<BlobDataHandle> blob_handle) {
  new BlobURLLoader(std::move(url_loader_receiver), method, headers,
                    std::move(client), std::move(blob_handle));
}

BlobURLLoader::~BlobURLLoader() = default;

BlobURLLoader::BlobURLLoader(
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    std::unique_ptr<BlobDataHandle> blob_handle)
    : receiver_(this, std::move(url_loader_receiver)),
      client_(std::move(client)),
      blob_handle_(std::move(blob_handle)) {
  // PostTask since it might destruct.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BlobURLLoader::Start,
                                weak_factory_.GetWeakPtr(), method, headers));
}

void BlobURLLoader::Start(const std::string& method,
                          const net::HttpRequestHeaders& headers) {
  if (!blob_handle_) {
    OnComplete(net::ERR_FILE_NOT_FOUND, 0);
    delete this;
    return;
  }

  // We only support GET request per the spec.
  if (method != "GET") {
    OnComplete(net::ERR_METHOD_NOT_SUPPORTED, 0);
    delete this;
    return;
  }

  if (std::optional<std::string> range_header =
          headers.GetHeader(net::HttpRequestHeaders::kRange);
      range_header) {
    // We only care about "Range" header here.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header.value(), &ranges)) {
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
  options.capacity_num_bytes =
      blink::BlobUtils::GetDataPipeCapacity(blob_handle_->size());
  if (mojo::CreateDataPipe(&options, producer_handle, consumer_handle) !=
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
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
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

  HeadersCompleted(status_code, content_size, std::nullopt);
  return DONT_REQUEST_SIDE_DATA;
}

void BlobURLLoader::DidReadSideData(std::optional<mojo_base::BigBuffer> data) {
  HeadersCompleted(net::HTTP_OK, total_size_, std::move(data));
}

void BlobURLLoader::OnComplete(net::Error error_code,
                               uint64_t total_written_bytes) {
  network::URLLoaderCompletionStatus status(error_code);
  status.encoded_body_length = total_written_bytes;
  status.decoded_body_length = total_written_bytes;
  client_->OnComplete(status);
}
void BlobURLLoader::HeadersCompleted(
    net::HttpStatusCode status_code,
    uint64_t content_size,
    std::optional<mojo_base::BigBuffer> metadata) {
  auto response = network::mojom::URLResponseHead::New();
  response->content_length = 0;
  if (status_code == net::HTTP_OK || status_code == net::HTTP_PARTIAL_CONTENT)
    response->content_length = content_size;
  response->headers = GenerateHeaders(status_code, blob_handle_.get(),
                                      &byte_range_, total_size_, content_size);

  std::string mime_type;
  response->headers->GetMimeType(&mime_type);
  if (mime_type.empty())
    mime_type = "text/plain";
  response->mime_type = mime_type;
  response->headers->GetCharset(&response->charset);

  // TODO(jam): some of this code can be shared with
  // services/network/url_loader.h

  client_->OnReceiveResponse(std::move(response),
                             std::move(response_body_consumer_handle_),
                             std::move(metadata));
  sent_headers_ = true;
}

}  // namespace storage
