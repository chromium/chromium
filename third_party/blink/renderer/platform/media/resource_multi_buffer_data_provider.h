// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_RESOURCE_MULTI_BUFFER_DATA_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_RESOURCE_MULTI_BUFFER_DATA_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/renderer/platform/media/multi_buffer.h"
#include "third_party/blink/renderer/platform/media/url_index.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

PLATFORM_EXPORT BASE_DECLARE_FEATURE(kDeferMediaAcceptEncodingInjection);

class WebAssociatedURLLoader;

class PLATFORM_EXPORT ResourceMultiBufferDataProvider
    : public MultiBuffer::DataProvider,
      public WebAssociatedURLLoaderClient {
 public:
  // NUmber of times we'll retry if the connection fails.
  enum { kMaxRetries = 30 };

  // Per-request redirect tracking state, reset at the start of each network
  // request in `Start()`.
  struct RedirectState {
    // Post-redirect URL if the current request followed at least one HTTP
    // redirect; empty otherwise.
    KURL redirects_to;

    // Worst-case relation (`kSameOrigin < kSameSite < kCrossSite`) of any URL
    // in the current request's redirect chain relative to
    // `url_data_->security_origin()`.
    net::OriginRelation max_origin_relation = net::OriginRelation::kSameOrigin;

    // True if any redirect hop in the current request crossed origins.
    bool has_cross_origin_redirect = false;

    // Service worker bypass mode for retargeted follow-up range requests.
    // Matching `blink::ResourceLoader::WillFollowRedirect`, any redirect hop
    // not synthesized by a Service Worker latches `kSkip` when
    // `media::kDisallowMixedOpaqueMediaResponses` is enabled.
    UrlData::ServiceWorkerMode service_worker_mode =
        UrlData::ServiceWorkerMode::kAllow;
  };

  ResourceMultiBufferDataProvider(
      UrlData* url_data,
      MultiBufferBlockId pos,
      bool is_client_audio_element,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ResourceMultiBufferDataProvider() override;

  // Virtual for testing purposes.
  virtual void Start();

  // MultiBuffer::DataProvider implementation
  MultiBufferBlockId Tell() const override;
  bool Available() const override;
  int64_t AvailableBytes() const override;
  scoped_refptr<media::DataBuffer> Read() override;
  void SetDeferred(bool defer) override;

  // WebAssociatedURLLoaderClient implementation.
  bool WillFollowRedirect(const WebURL& new_url,
                          const WebURLResponse& redirect_response) override;
  void DidSendData(uint64_t bytes_sent,
                   uint64_t total_bytes_to_be_sent) override;
  void DidReceiveResponse(const WebURLResponse& response) override;
  void DidDownloadData(uint64_t data_length) override;
  void DidReceiveData(base::span<const char> data_length) override;
  void DidFinishLoading() override;
  void DidFail(const WebURLError&) override;

  // Use protected instead of private for testing purposes.
 protected:
  friend class MultiBufferDataSourceTest;
  friend class ResourceMultiBufferDataProviderTest;

  // Callback used when we're asked to fetch data after the end of the file.
  void Terminate();

  // Parse a Content-Range header into its component pieces and return true if
  // each of the expected elements was found & parsed correctly.
  // |*instance_size| may be set to kPositionNotSpecified if the range ends in
  // "/*".
  // NOTE: only public for testing!  This is an implementation detail of
  // VerifyPartialResponse (a private method).
  static bool ParseContentRange(const std::string& content_range_str,
                                int64_t* first_byte_position,
                                int64_t* last_byte_position,
                                int64_t* instance_size);

  int64_t byte_pos() const;
  int64_t block_size() const;
  bool HasBufferedData() const;

  // Resets `active_loader_` and fails `url_data_` (using `CrossOriginFail()`
  // when `cross_origin_failure` is true).
  void FailForInvalidResponse(bool cross_origin_failure);

  // Validates a 206 Partial Content response's `Content-Range` header against
  // `byte_pos()` and any existing length on `url_data_` and
  // `destination_url_data`. On success, populates `*instance_size` and
  // `*bytes_to_discard`.
  bool VerifyPartialResponse(const WebURLResponse& response,
                             const UrlData& destination_url_data,
                             int64_t* instance_size,
                             uint64_t* bytes_to_discard) const;

  // Validates a 200 OK response's `content_length` against any existing length
  // on `url_data_` and `destination_url_data`, rejecting mid-resource opaque
  // service worker 200 OK responses.
  bool VerifyOkResponse(const UrlData& destination_url_data,
                        int64_t content_length,
                        bool is_cors_cross_origin,
                        bool was_fetched_via_service_worker) const;

  // Validates a 416 Range Not Satisfiable response, rejecting opaque or
  // mid-resource (`byte_pos() < length`) 416 responses and verifying length
  // compatibility between `url_data_` and `destination_url_data`.
  bool VerifyRangeNotSatisfiableResponse(const UrlData& destination_url_data,
                                         bool is_cors_cross_origin) const;

  // Returns true if `length` is compatible with any existing length on both
  // `url_data_` and `destination_url_data`.
  bool HasCompatibleLengths(const UrlData& destination_url_data,
                            int64_t length) const;

  // Current Position.
  MultiBufferBlockId pos_;

  // This is where we actually get read data from.
  // We don't need (or want) a scoped_refptr for this one, because
  // we are owned by it. Note that we may change this when we encounter
  // a redirect because we actually change ownership.
  raw_ptr<UrlData> url_data_;

  // Temporary storage for incoming data.
  std::list<scoped_refptr<media::DataBuffer>> fifo_;

  // How many retries have we done at the current position.
  int retries_;

  // Copy of url_data_->cors_mode()
  // const to make it obvious that redirects cannot change it.
  const UrlData::CorsMode cors_mode_;

  // Keeps track of an active WebAssociatedURLLoader.
  // Only valid while loading resource.
  std::unique_ptr<WebAssociatedURLLoader> active_loader_;

  // Tracks redirects followed by the current request. Reset on `Start()`.
  RedirectState redirect_state_;

  // If the server tries to gives us more bytes than we want, this how
  // many bytes we need to discard before we get to the right place.
  uint64_t bytes_to_discard_ = 0;

  // Is the client an audio element?
  bool is_client_audio_element_ = false;

  // True if any response handled by this data provider was fetched via a
  // service worker.
  bool was_fetched_via_service_worker_ = false;

  // True if the current request received any payload bytes past
  // `bytes_to_discard_`. Reset on `Start()`.
  bool received_data_ = false;

  size_t total_bytes_received_ = 0;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<ResourceMultiBufferDataProvider> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_RESOURCE_MULTI_BUFFER_DATA_PROVIDER_H_
