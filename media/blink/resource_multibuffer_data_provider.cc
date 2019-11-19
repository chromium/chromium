// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/resource_multibuffer_data_provider.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/blink/cache_util.h"
#include "media/blink/media_blink_export.h"
#include "media/blink/resource_fetch_context.h"
#include "media/blink/url_index.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/cors/cors.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"

using blink::WebAssociatedURLLoader;
using blink::WebString;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace media {

// The number of milliseconds to wait before retrying a failed load.
const int kLoaderFailedRetryDelayMs = 250;

// Each retry, add this many MS to the delay.
// total delay is:
// (kLoaderPartialRetryDelayMs +
//  kAdditionalDelayPerRetryMs * (kMaxRetries - 1) / 2) * kMaxretries = 29250 ms
const int kAdditionalDelayPerRetryMs = 50;

// The number of milliseconds to wait before retrying when the server
// decides to not give us all the data at once.
const int kLoaderPartialRetryDelayMs = 25;

const int kHttpOK = 200;
const int kHttpPartialContent = 206;
const int kHttpRangeNotSatisfiable = 416;

ResourceMultiBufferDataProvider::ResourceMultiBufferDataProvider(
    UrlData* url_data,
    MultiBufferBlockId pos,
    bool is_client_audio_element)
    : pos_(pos),
      url_data_(url_data),
      retries_(0),
      cors_mode_(url_data->cors_mode()),
      origin_(url_data->url().GetOrigin()),
      is_client_audio_element_(is_client_audio_element) {
  DCHECK(url_data_) << " pos = " << pos;
  DCHECK_GE(pos, 0);
}

ResourceMultiBufferDataProvider::~ResourceMultiBufferDataProvider() = default;

void ResourceMultiBufferDataProvider::Start() {
  DVLOG(1) << __func__ << " @ " << byte_pos();
  if (url_data_->length() > 0 && byte_pos() >= url_data_->length()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ResourceMultiBufferDataProvider::Terminate,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  // Prepare the request.
  WebURLRequest request(url_data_->url());
  request.SetRequestContext(is_client_audio_element_
                                ? blink::mojom::RequestContextType::AUDIO
                                : blink::mojom::RequestContextType::VIDEO);
  request.SetHttpHeaderField(
      WebString::FromUTF8(net::HttpRequestHeaders::kRange),
      WebString::FromUTF8(
          net::HttpByteRange::RightUnbounded(byte_pos()).GetHeaderValue()));

  if (url_data_->length() == kPositionNotSpecified &&
      url_data_->CachedSize() == 0 && url_data_->BytesReadFromCache() == 0 &&
      blink::WebNetworkStateNotifier::SaveDataEnabled() &&
      url_data_->url().SchemeIs(url::kHttpScheme)) {
    // This lets the data reduction proxy know that we don't have anything
    // previously cached data for this resource. We can only send it if this is
    // the first request for this resource.
    request.SetHttpHeaderField(WebString::FromUTF8("chrome-proxy"),
                               WebString::FromUTF8("frfr"));
  }

  // We would like to send an if-match header with the request to
  // tell the remote server that we really can't handle files other
  // than the one we already started playing. Unfortunately, doing
  // so will disable the http cache, and possibly other proxies
  // along the way. See crbug/504194 and crbug/689989 for more information.

  // Disable compression, compression for audio/video doesn't make sense...
  request.SetHttpHeaderField(
      WebString::FromUTF8(net::HttpRequestHeaders::kAcceptEncoding),
      WebString::FromUTF8("identity;q=1, *;q=0"));

  // Start resource loading.
  blink::WebAssociatedURLLoaderOptions options;
  if (url_data_->cors_mode() != UrlData::CORS_UNSPECIFIED) {
    options.expose_all_response_headers = true;
    // The author header set is empty, no preflight should go ahead.
    options.preflight_policy =
        network::mojom::CorsPreflightPolicy::kPreventPreflight;

    request.SetMode(network::mojom::RequestMode::kCors);
    if (url_data_->cors_mode() != UrlData::CORS_USE_CREDENTIALS) {
      request.SetCredentialsMode(network::mojom::CredentialsMode::kSameOrigin);
    }
  }

  active_loader_ =
      url_data_->url_index()->fetch_context()->CreateUrlLoader(options);
  active_loader_->LoadAsynchronously(request, this);
}

/////////////////////////////////////////////////////////////////////////////
// MultiBuffer::DataProvider implementation.
MultiBufferBlockId ResourceMultiBufferDataProvider::Tell() const {
  return pos_;
}

bool ResourceMultiBufferDataProvider::Available() const {
  if (fifo_.empty())
    return false;
  if (fifo_.back()->end_of_stream())
    return true;
  if (fifo_.front()->data_size() == block_size())
    return true;
  return false;
}

int64_t ResourceMultiBufferDataProvider::AvailableBytes() const {
  int64_t bytes = 0;
  for (const auto i : fifo_) {
    if (i->end_of_stream())
      break;
    bytes += i->data_size();
  }
  return bytes;
}

scoped_refptr<DataBuffer> ResourceMultiBufferDataProvider::Read() {
  DCHECK(Available());
  scoped_refptr<DataBuffer> ret = fifo_.front();
  fifo_.pop_front();
  ++pos_;
  return ret;
}

void ResourceMultiBufferDataProvider::SetDeferred(bool deferred) {
  if (active_loader_)
    active_loader_->SetDefersLoading(deferred);
}

/////////////////////////////////////////////////////////////////////////////
// WebAssociatedURLLoaderClient implementation.

bool ResourceMultiBufferDataProvider::WillFollowRedirect(
    const blink::WebURL& new_url,
    const WebURLResponse& redirect_response) {
  DVLOG(1) << "willFollowRedirect";
  redirects_to_ = new_url;
  url_data_->set_valid_until(base::Time::Now() +
                             GetCacheValidUntil(redirect_response));

  // This test is vital for security!
  if (cors_mode_ == UrlData::CORS_UNSPECIFIED) {
    // We allow the redirect if the origin is the same.
    if (origin_ != redirects_to_.GetOrigin()) {
      // We also allow the redirect if we don't have any data in the
      // cache, as that means that no dangerous data mixing can occur.
      if (url_data_->multibuffer()->map().empty() && fifo_.empty())
        return true;

      active_loader_.reset();
      url_data_->Fail();
      return false;  // "this" may be deleted now.
    }
  }
  return true;
}

void ResourceMultiBufferDataProvider::DidSendData(
    uint64_t bytes_sent,
    uint64_t total_bytes_to_be_sent) {
  NOTIMPLEMENTED();
}

void ResourceMultiBufferDataProvider::DidReceiveResponse(
    const WebURLResponse& response) {
#if DCHECK_IS_ON()
  std::string version;
  switch (response.HttpVersion()) {
    case WebURLResponse::kHTTPVersion_0_9:
      version = "0.9";
      break;
    case WebURLResponse::kHTTPVersion_1_0:
      version = "1.0";
      break;
    case WebURLResponse::kHTTPVersion_1_1:
      version = "1.1";
      break;
    case WebURLResponse::kHTTPVersion_2_0:
      version = "2.1";
      break;
    case WebURLResponse::kHTTPVersionUnknown:
      version = "unknown";
      break;
  }
  DVLOG(1) << "didReceiveResponse: HTTP/" << version << " "
           << response.HttpStatusCode();
#endif
  DCHECK(active_loader_);

  scoped_refptr<UrlData> destination_url_data(url_data_);

  if (!redirects_to_.is_empty()) {
    destination_url_data =
        url_data_->url_index()->GetByUrl(redirects_to_, cors_mode_);
    redirects_to_ = GURL();
  }

  base::Time last_modified;
  if (base::Time::FromString(
          response.HttpHeaderField("Last-Modified").Utf8().data(),
          &last_modified)) {
    destination_url_data->set_last_modified(last_modified);
  }

  destination_url_data->set_etag(
      response.HttpHeaderField("ETag").Utf8().data());

  destination_url_data->set_valid_until(base::Time::Now() +
                                        GetCacheValidUntil(response));

  destination_url_data->set_cacheable(GetReasonsForUncacheability(response) ==
                                      0);

  // Expected content length can be |kPositionNotSpecified|, in that case
  // |content_length_| is not specified and this is a streaming response.
  int64_t content_length = response.ExpectedContentLength();
  bool end_of_file = false;
  bool do_fail = false;
  // We get the response type here because aborting the loader may change it.
  const auto response_type = response.GetType();
  bytes_to_discard_ = 0;

  // We make a strong assumption that when we reach here we have either
  // received a response from HTTP/HTTPS protocol or the request was
  // successful (in particular range request). So we only verify the partial
  // response for HTTP and HTTPS protocol.
  if (destination_url_data->url().SchemeIsHTTPOrHTTPS()) {
    bool partial_response = (response.HttpStatusCode() == kHttpPartialContent);
    bool ok_response = (response.HttpStatusCode() == kHttpOK);

    // Check to see whether the server supports byte ranges.
    std::string accept_ranges =
        response.HttpHeaderField("Accept-Ranges").Utf8();
    if (accept_ranges.find("bytes") != std::string::npos)
      destination_url_data->set_range_supported();

    // If we have verified the partial response and it is correct.
    // It's also possible for a server to support range requests
    // without advertising "Accept-Ranges: bytes".
    if (partial_response &&
        VerifyPartialResponse(response, destination_url_data)) {
      destination_url_data->set_range_supported();
    } else if (ok_response) {
      // We accept a 200 response for a Range:0- request, trusting the
      // Accept-Ranges header, because Apache thinks that's a reasonable thing
      // to return.
      destination_url_data->set_length(content_length);
      bytes_to_discard_ = byte_pos();
    } else if (response.HttpStatusCode() == kHttpRangeNotSatisfiable) {
      // Unsatisfiable range
      // Really, we should never request a range that doesn't exist, but
      // if we do, let's handle it in a sane way.
      // Note, we can't just call OnDataProviderEvent() here, because
      // url_data_ hasn't been updated to the final destination yet.
      end_of_file = true;
    } else {
      active_loader_.reset();
      // Can't call fail until readers have been migrated to the new
      // url data below.
      do_fail = true;
    }
  } else {
    destination_url_data->set_range_supported();
    if (content_length != kPositionNotSpecified) {
      destination_url_data->set_length(content_length + byte_pos());
    }
  }

  if (!do_fail) {
    destination_url_data =
        url_data_->url_index()->TryInsert(destination_url_data);
  }

  // This is vital for security!
  destination_url_data->set_is_cors_cross_origin(
      network::cors::IsCorsCrossOriginResponseType(response_type));

  // Only used for metrics.
  {
    WebString access_control =
        response.HttpHeaderField("Access-Control-Allow-Origin");
    if (!access_control.IsEmpty() && !access_control.Equals("null")) {
      // Note: When |access_control| is not *, we should verify that it matches
      // the requesting origin. Instead we just assume that it matches, which is
      // probably accurate enough for metrics.
      destination_url_data->set_has_access_control();
    }
  }

  if (destination_url_data != url_data_) {
    // At this point, we've encountered a redirect, or found a better url data
    // instance for the data that we're about to download.

    // First, let's take a ref on the current url data.
    scoped_refptr<UrlData> old_url_data(url_data_);
    destination_url_data->Use();

    // Take ownership of ourselves. (From the multibuffer)
    std::unique_ptr<DataProvider> self(
        url_data_->multibuffer()->RemoveProvider(this));
    url_data_ = destination_url_data.get();
    // Give the ownership to our new owner.
    url_data_->multibuffer()->AddProvider(std::move(self));

    // Call callback to let upstream users know about the transfer.
    // This will merge the data from the two multibuffers and
    // cause clients to start using the new UrlData.
    old_url_data->RedirectTo(destination_url_data);
  }

  if (do_fail) {
    destination_url_data->Fail();
    return;  // "this" may be deleted now.
  }

  // Get the response URL since it can differ from the request URL when a
  // service worker provided the response. Normally we would just use
  // ResponseUrl(), but ResourceMultibufferDataProvider disallows mixing
  // constructed responses (new Response()) and native server responses, even if
  // they have the same response URL.
  GURL response_url;
  if (!response.WasFetchedViaServiceWorker() ||
      response.HasUrlListViaServiceWorker()) {
    response_url = response.ResponseUrl();
  }

  // This test is vital for security!
  if (!url_data_->ValidateDataOrigin(response_url.GetOrigin())) {
    active_loader_.reset();
    url_data_->Fail();
    return;  // "this" may be deleted now.
  }

  if (end_of_file) {
    fifo_.push_back(DataBuffer::CreateEOSBuffer());
    url_data_->multibuffer()->OnDataProviderEvent(this);
  }
}

void ResourceMultiBufferDataProvider::DidReceiveData(const char* data,
                                                     int data_length) {
  DVLOG(1) << "didReceiveData: " << data_length << " bytes";
  DCHECK(!Available());
  DCHECK(active_loader_);
  DCHECK_GT(data_length, 0);

  if (bytes_to_discard_) {
    uint64_t tmp = std::min<uint64_t>(bytes_to_discard_, data_length);
    data_length -= tmp;
    data += tmp;
    bytes_to_discard_ -= tmp;
    if (data_length == 0)
      return;
  }

  // When we receive data, we allow more retries.
  retries_ = 0;

  while (data_length) {
    if (fifo_.empty() || fifo_.back()->data_size() == block_size()) {
      fifo_.push_back(new DataBuffer(block_size()));
      fifo_.back()->set_data_size(0);
    }
    int last_block_size = fifo_.back()->data_size();
    int to_append = std::min<int>(data_length, block_size() - last_block_size);
    DCHECK_GT(to_append, 0);
    memcpy(fifo_.back()->writable_data() + last_block_size, data, to_append);
    data += to_append;
    fifo_.back()->set_data_size(last_block_size + to_append);
    data_length -= to_append;
  }

  url_data_->multibuffer()->OnDataProviderEvent(this);

  // Beware, this object might be deleted here.
}

void ResourceMultiBufferDataProvider::DidDownloadData(uint64_t dataLength) {
  NOTIMPLEMENTED();
}

void ResourceMultiBufferDataProvider::DidReceiveCachedMetadata(
    const char* data,
    int data_length) {
  NOTIMPLEMENTED();
}

void ResourceMultiBufferDataProvider::DidFinishLoading() {
  DVLOG(1) << "didFinishLoading";
  DCHECK(active_loader_.get());
  DCHECK(!Available());

  // We're done with the loader.
  active_loader_.reset();

  // If we didn't know the |instance_size_| we do now.
  int64_t size = byte_pos();

  // This request reports something smaller than what we've seen in the past,
  // Maybe it's transient error?
  if (url_data_->length() != kPositionNotSpecified &&
      size < url_data_->length()) {
    if (retries_ < kMaxRetries) {
      DVLOG(1) << " Partial data received.... @ pos = " << size;
      retries_++;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ResourceMultiBufferDataProvider::Start,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kLoaderPartialRetryDelayMs));
      return;
    } else {
      url_data_->Fail();
      return;  // "this" may be deleted now.
    }
  }

  url_data_->set_length(size);
  fifo_.push_back(DataBuffer::CreateEOSBuffer());

  if (url_data_->url_index()) {
    url_data_->url_index()->TryInsert(url_data_);
  }

  DCHECK(Available());
  url_data_->multibuffer()->OnDataProviderEvent(this);

  // Beware, this object might be deleted here.
}

void ResourceMultiBufferDataProvider::DidFail(const WebURLError& error) {
  DVLOG(1) << "didFail: reason=" << error.reason();
  DCHECK(active_loader_.get());
  active_loader_.reset();

  if (retries_ < kMaxRetries && pos_ != 0) {
    retries_++;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ResourceMultiBufferDataProvider::Start,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kLoaderFailedRetryDelayMs + kAdditionalDelayPerRetryMs * retries_));
  } else {
    // We don't need to continue loading after failure.
    // Note that calling Fail() will most likely delete this object.
    url_data_->Fail();
  }
}

bool ResourceMultiBufferDataProvider::ParseContentRange(
    const std::string& content_range_str,
    int64_t* first_byte_position,
    int64_t* last_byte_position,
    int64_t* instance_size) {
  const char kUpThroughBytesUnit[] = "bytes ";
  if (!base::StartsWith(content_range_str, kUpThroughBytesUnit,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }
  std::string range_spec =
      content_range_str.substr(sizeof(kUpThroughBytesUnit) - 1);
  size_t dash_offset = range_spec.find("-");
  size_t slash_offset = range_spec.find("/");

  if (dash_offset == std::string::npos || slash_offset == std::string::npos ||
      slash_offset < dash_offset || slash_offset + 1 == range_spec.length()) {
    return false;
  }
  if (!base::StringToInt64(range_spec.substr(0, dash_offset),
                           first_byte_position) ||
      !base::StringToInt64(
          range_spec.substr(dash_offset + 1, slash_offset - dash_offset - 1),
          last_byte_position)) {
    return false;
  }
  if (slash_offset == range_spec.length() - 2 &&
      range_spec[slash_offset + 1] == '*') {
    *instance_size = kPositionNotSpecified;
  } else {
    if (!base::StringToInt64(range_spec.substr(slash_offset + 1),
                             instance_size)) {
      return false;
    }
  }
  if (*last_byte_position < *first_byte_position ||
      (*instance_size != kPositionNotSpecified &&
       *last_byte_position >= *instance_size)) {
    return false;
  }

  return true;
}

void ResourceMultiBufferDataProvider::Terminate() {
  fifo_.push_back(DataBuffer::CreateEOSBuffer());
  url_data_->multibuffer()->OnDataProviderEvent(this);
}

int64_t ResourceMultiBufferDataProvider::byte_pos() const {
  int64_t ret = pos_;
  ret += fifo_.size();
  ret = ret << url_data_->multibuffer()->block_size_shift();
  if (!fifo_.empty()) {
    ret += fifo_.back()->data_size() - block_size();
  }
  return ret;
}

int64_t ResourceMultiBufferDataProvider::block_size() const {
  int64_t ret = 1;
  return ret << url_data_->multibuffer()->block_size_shift();
}

bool ResourceMultiBufferDataProvider::VerifyPartialResponse(
    const WebURLResponse& response,
    const scoped_refptr<UrlData>& url_data) {
  int64_t first_byte_position, last_byte_position, instance_size;
  if (!ParseContentRange(response.HttpHeaderField("Content-Range").Utf8(),
                         &first_byte_position, &last_byte_position,
                         &instance_size)) {
    return false;
  }

  if (url_data_->length() == kPositionNotSpecified) {
    url_data->set_length(instance_size);
  }

  if (first_byte_position > byte_pos()) {
    return false;
  }
  if (last_byte_position + 1 < byte_pos()) {
    return false;
  }
  bytes_to_discard_ = byte_pos() - first_byte_position;

  return true;
}

}  // namespace media
