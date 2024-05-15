// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/mock_signed_exchange_handler.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_entry.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "net/filter/source_stream.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace content {

namespace {

// A mostly pass-through wrapper around another `net::SourceStream`, except that
// the wrapper strips an initial sequence of bytes (called the `prefix_to_strip`
// in the constructor).
//
// The wrapper is used to recover the inner resource from the body of a mocked
// SXG payload, because the mocked SXG payload is prefixed with `kMockSxgPrefix`
// so that it doesn't sniff as HTML when processed by ORB.  SXG in general does
// not look like HTML (because of CBOR/binary encoding [1]), so this kind of
// prefixing is somewhat desirable in general, even though real SXGs don't
// really begin with `kMockSxgPrefix` bytes.  This might remain desirable even
// once SXG prefetches switch from `no-cors` to `cors` mode (see
// https://crbug.com/1316660).
//
// [1] https://web.dev/signed-exchanges/#the-sxg-format
class PrefixStrippingSourceStream : public net::SourceStream {
 public:
  PrefixStrippingSourceStream(std::string_view prefix_to_strip,
                              std::unique_ptr<net::SourceStream> stream_to_wrap)
      : net::SourceStream(stream_to_wrap->type()),
        remaining_prefix_to_strip_(prefix_to_strip),
        wrapped_stream_(std::move(stream_to_wrap)),
        weak_factory_(this) {
    DCHECK(wrapped_stream_);
  }

  ~PrefixStrippingSourceStream() override = default;

  int Read(net::IOBuffer* dest_buffer,
           int buffer_size,
           net::CompletionOnceCallback callback) override {
    DCHECK(dest_buffer);
    DCHECK_GT(buffer_size, 0);
    DCHECK(callback);

    // `callback` might sometimes need to survive more than 1 wrapped `Read`.
    // To make this easier to handle, `callback` is put into a ref-counted
    // PendingRead struct.  (Performance implications of an extra heap
    // allocation are ignored for this test-only code.)
    auto pending_read = base::MakeRefCounted<PendingRead>();
    pending_read->dest_buffer = dest_buffer;
    pending_read->buffer_size = buffer_size;
    pending_read->callback = std::move(callback);

    return Read(pending_read);
  }

  std::string Description() const override {
    return wrapped_stream_->Description();
  }

  bool MayHaveMoreBytes() const override {
    return wrapped_stream_->MayHaveMoreBytes();
  }

 private:
  // Stores arguments of a call to the public `PrefixStrippingSourceStream`'s
  // `Read` method (during duration of 1 or more sync-or-async calls to the
  // `wrapped_stream_`'s `Read` method, while we are waiting to read and skip
  // the initial `prefix_to_strip` bytes).
  struct PendingRead : public base::RefCountedThreadSafe<PendingRead> {
    scoped_refptr<net::IOBuffer> dest_buffer;
    int buffer_size;
    net::CompletionOnceCallback callback;

   private:
    friend class base::RefCountedThreadSafe<PendingRead>;
    virtual ~PendingRead() = default;
  };

  int Read(const scoped_refptr<PendingRead>& pending_read) {
    DCHECK(pending_read);

    if (remaining_prefix_to_strip_.empty()) {
      return wrapped_stream_->Read(pending_read->dest_buffer.get(),
                                   pending_read->buffer_size,
                                   std::move(pending_read->callback));
    }

    int number_of_bytes_read = wrapped_stream_->Read(
        pending_read->dest_buffer.get(), pending_read->buffer_size,
        base::BindOnce(&PrefixStrippingSourceStream::OnWrappedReadCompleted,
                       weak_factory_.GetWeakPtr(), pending_read));
    int number_of_post_prefix_bytes_read =
        ConsumeWrappedRead(pending_read, number_of_bytes_read);
    return number_of_post_prefix_bytes_read;
  }

  void OnWrappedReadCompleted(const scoped_refptr<PendingRead>& pending_read,
                              int number_of_bytes_read) {
    DCHECK(pending_read);
    DCHECK_NE(number_of_bytes_read, net::ERR_IO_PENDING);

    int number_of_post_prefix_bytes_read =
        ConsumeWrappedRead(pending_read, number_of_bytes_read);
    if (number_of_post_prefix_bytes_read != net::ERR_IO_PENDING) {
      std::move(pending_read->callback).Run(number_of_post_prefix_bytes_read);
    }
  }

  // Processes the results of a sync or async call to the `wrapped_stream_`'s
  // Read method:
  // 1. Consumes bytes read from the `wrapped_stream_`, discarding them as long
  //    as they match the `remaining_prefix_to_strip_`.
  // 2. If needed, starts more `Read`s from the `wrapped_stream_` and only
  //    returns a non-negative integer after post-prefix bytes are available.
  int ConsumeWrappedRead(const scoped_refptr<PendingRead>& pending_read,
                         int number_of_bytes_read) {
    DCHECK(pending_read);

    // Propagate errors (and ERR_IO_PENDING indicator of async results).
    // Only consider successful reads below.
    if (number_of_bytes_read < 0) {
      return number_of_bytes_read;
    }

    // Strip `remaining_prefix_to_strip_` bytes from `bytes_read`.
    std::string_view bytes_read(pending_read->dest_buffer->data(),
                                number_of_bytes_read);
    int maybe_consumed_bytes =
        std::min(bytes_read.size(), remaining_prefix_to_strip_.size());
    CHECK_EQ(remaining_prefix_to_strip_.substr(0, maybe_consumed_bytes),
             bytes_read.substr(0, maybe_consumed_bytes))
        << "Unexpectedly mismatched prefix - can't continue (because some "
        << "of the wrapped bytes may have been already discarded earlier)";
    remaining_prefix_to_strip_.remove_prefix(maybe_consumed_bytes);
    bytes_read.remove_prefix(maybe_consumed_bytes);

    // If ready, then return the number of real, post-prefix bytes read.
    //
    // Care is taken to avoid incorrectly reporting an EOF (zero bytes read)
    // right after stripping the prefix, but before reading all the data from
    // the `wrapped_stream_`.
    bool maybe_incorrect_eof = bytes_read.empty() && MayHaveMoreBytes();
    if (remaining_prefix_to_strip_.empty() && !maybe_incorrect_eof) {
      // Source and destination may overlap - need to use `memmove`.
      memmove(pending_read->dest_buffer->data(), bytes_read.data(),
              bytes_read.size());
      return bytes_read.size();
    }

    // Still have `remaining_prefix_to_strip_` - need to `Read` more bytes from
    // the `wrapped_stream_`.
    return Read(pending_read);
  }

  std::string_view remaining_prefix_to_strip_;
  const std::unique_ptr<net::SourceStream> wrapped_stream_;
  base::WeakPtrFactory<PrefixStrippingSourceStream> weak_factory_;
};

}  // namespace

MockSignedExchangeHandlerParams::MockSignedExchangeHandlerParams(
    const GURL& outer_url,
    SignedExchangeLoadResult result,
    net::Error error,
    const GURL& inner_url,
    const std::string& mime_type,
    std::vector<std::pair<std::string, std::string>> response_headers,
    const net::SHA256HashValue& header_integrity,
    const base::Time& signature_expire_time)
    : outer_url(outer_url),
      result(result),
      error(error),
      inner_url(inner_url),
      mime_type(mime_type),
      response_headers(std::move(response_headers)),
      header_integrity(header_integrity),
      signature_expire_time(signature_expire_time.is_null()
                                ? base::Time::Now() + base::Days(1)
                                : signature_expire_time) {}

MockSignedExchangeHandlerParams::MockSignedExchangeHandlerParams(
    const MockSignedExchangeHandlerParams& other) = default;
MockSignedExchangeHandlerParams::~MockSignedExchangeHandlerParams() = default;

MockSignedExchangeHandler::MockSignedExchangeHandler(
    const MockSignedExchangeHandlerParams& params,
    std::unique_ptr<net::SourceStream> body,
    ExchangeHeadersCallback headers_callback)
    : header_integrity_(params.header_integrity),
      signature_expire_time_(params.signature_expire_time),
      cert_url_(params.outer_url.Resolve("mock_cert")),
      cert_server_ip_address_(net::IPAddress::IPv4Localhost()) {
  auto head = network::mojom::URLResponseHead::New();
  if (params.error == net::OK) {
    head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    head->mime_type = params.mime_type;
    head->headers->SetHeader("Content-type", params.mime_type);
    for (const auto& header : params.response_headers)
      head->headers->AddHeader(header.first, header.second);
    head->is_signed_exchange_inner_response = true;
    head->content_length = head->headers->GetContentLength();
  }
  body = std::make_unique<PrefixStrippingSourceStream>(kMockSxgPrefix,
                                                       std::move(body));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(headers_callback), params.result, params.error,
                     params.inner_url, std::move(head), std::move(body)));
}

bool MockSignedExchangeHandler::GetSignedExchangeInfoForPrefetchCache(
    PrefetchedSignedExchangeCacheEntry& entry) const {
  entry.SetHeaderIntegrity(
      std::make_unique<net::SHA256HashValue>(header_integrity_));
  entry.SetSignatureExpireTime(signature_expire_time_);
  entry.SetCertUrl(cert_url_);
  entry.SetCertServerIPAddress(cert_server_ip_address_);
  return true;
}

MockSignedExchangeHandler::~MockSignedExchangeHandler() {}

MockSignedExchangeHandlerFactory::MockSignedExchangeHandlerFactory(
    std::vector<MockSignedExchangeHandlerParams> params_list)
    : params_list_(std::move(params_list)) {}

MockSignedExchangeHandlerFactory::~MockSignedExchangeHandlerFactory() = default;

std::unique_ptr<SignedExchangeHandler> MockSignedExchangeHandlerFactory::Create(
    const GURL& outer_url,
    std::unique_ptr<net::SourceStream> body,
    ExchangeHeadersCallback headers_callback,
    std::unique_ptr<SignedExchangeCertFetcherFactory> cert_fetcher_factory) {
  for (const auto& params : params_list_) {
    if (params.outer_url == outer_url) {
      return std::make_unique<MockSignedExchangeHandler>(
          params, std::move(body), std::move(headers_callback));
    }
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace content
