// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_ui_loading_util.h"

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

namespace webui {

namespace {

class MemoryDataSource : public mojo::DataPipeProducer::DataSource {
 public:
  MemoryDataSource(scoped_refptr<base::RefCountedMemory> data,
                   size_t offset,
                   size_t size)
      : data_(std::move(data)), offset_(offset), size_(size) {}

  uint64_t GetLength() const override { return size_; }

  ReadResult Read(uint64_t offset, base::span<char> buffer) override {
    ReadResult result;
    CHECK_LT(offset, size_);
    size_t copy_size =
        std::min(buffer.size(), size_ - static_cast<size_t>(offset));
    base::as_writable_bytes(buffer).copy_prefix_from(base::span(*data_).subspan(
        offset_ + static_cast<size_t>(offset), copy_size));
    result.bytes_read = copy_size;
    result.result = MOJO_RESULT_OK;
    return result;
  }

 private:
  scoped_refptr<base::RefCountedMemory> data_;
  size_t offset_;
  size_t size_;
};

}  // namespace

void CallOnError(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    int error_code) {
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));

  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  client->OnComplete(status);
}

base::expected<net::HttpByteRange, GetRequestedRangeError> GetRequestedRange(
    const net::HttpRequestHeaders& headers) {
  std::optional<std::string> range_header =
      headers.GetHeader(net::HttpRequestHeaders::kRange);
  if (!range_header) {
    return base::unexpected(GetRequestedRangeError::kNoRanges);
  }
  std::vector<net::HttpByteRange> ranges;
  if (!net::HttpUtil::ParseRangeHeader(*range_header, &ranges)) {
    return base::unexpected(GetRequestedRangeError::kParseFailed);
  }
  if (ranges.size() > 1u) {
    return base::unexpected(GetRequestedRangeError::kMultipleRanges);
  }
  return ranges[0];
}

bool SendData(
    network::mojom::URLResponseHeadPtr headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    std::optional<net::HttpByteRange> requested_range,
    scoped_refptr<base::RefCountedMemory> bytes) {
  // The use of MojoCreateDataPipeOptions below means we'll be using uint32_t
  // for sizes / offsets.
  if (!base::IsValueInRangeForNumericType<uint32_t>(bytes->size())) {
    CallOnError(std::move(client_remote), net::ERR_INSUFFICIENT_RESOURCES);
    return false;
  }

  if (requested_range) {
    if (!requested_range->ComputeBounds(bytes->size())) {
      CallOnError(std::move(client_remote),
                  net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      return false;
    }
  }

  auto [pipe, output_size] = GetPipe(bytes, requested_range);

  // For media content, |content_length| must be known upfront for data that is
  // assumed to be fully buffered (as opposed to streamed from the network),
  // otherwise the media player will get confused and refuse to play.
  // Content delivered via chrome:// URLs is assumed fully buffered.
  headers->content_length = output_size;

  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));
  client->OnReceiveResponse(std::move(headers), std::move(pipe), std::nullopt);

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = output_size;
  status.encoded_body_length = output_size;
  status.decoded_body_length = output_size;
  client->OnComplete(status);
  return true;
}

std::pair<mojo::ScopedDataPipeConsumerHandle, size_t> GetPipe(
    scoped_refptr<base::RefCountedMemory> bytes,
    std::optional<net::HttpByteRange> requested_range) {
  CHECK(base::IsValueInRangeForNumericType<uint32_t>(bytes->size()));
  uint32_t output_offset = 0;
  size_t output_size = bytes->size();
  if (requested_range) {
    DCHECK(base::IsValueInRangeForNumericType<uint32_t>(
        requested_range->first_byte_position()))
        << "Expecting ComputeBounds() to enforce it";
    output_offset = requested_range->first_byte_position();
    output_size = requested_range->last_byte_position() -
                  requested_range->first_byte_position() + 1;
  }

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  // `0` means using the default capacity.
  options.capacity_num_bytes = 0;
  mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
  MojoResult create_result = mojo::CreateDataPipe(
      &options, pipe_producer_handle, pipe_consumer_handle);
  if (create_result != MOJO_RESULT_OK) {
    SCOPED_CRASH_KEY_NUMBER("WebUI", "mojo_CreateDataPipe_result",
                            create_result);
    CHECK(false);
  }

  auto producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(pipe_producer_handle));
  auto* producer_ptr = producer.get();
  producer_ptr->Write(
      std::make_unique<MemoryDataSource>(bytes, output_offset, output_size),
      base::DoNothingWithBoundArgs(std::move(producer)));

  return std::make_pair(std::move(pipe_consumer_handle), output_size);
}

}  // namespace webui

}  // namespace content
