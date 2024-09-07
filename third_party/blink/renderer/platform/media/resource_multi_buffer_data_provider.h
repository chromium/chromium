// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_RESOURCE_MULTI_BUFFER_DATA_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_RESOURCE_MULTI_BUFFER_DATA_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/media/multi_buffer.h"
#include "third_party/blink/renderer/platform/media/url_index.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebAssociatedURLLoader;

class PLATFORM_EXPORT ResourceMultiBufferDataProvider
    : public MultiBuffer::DataProvider,
      public WebAssociatedURLLoaderClient {
 public:
  // NUmber of times we'll retry if the connection fails.
  enum { kMaxRetries = 30 };

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
  void DidSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) override;
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

  // If we have made a range request, verify the response from the server.
  bool VerifyPartialResponse(const WebURLResponse& response,
                             const scoped_refptr<UrlData>& url_data);

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

  // The origin for the initial request.
  // const to make it obvious that redirects cannot change it.
  const GURL origin_ ALLOW_DISCOURAGED_TYPE("Avoids conversion in media code");

  // Keeps track of an active WebAssociatedURLLoader.
  // Only valid while loading resource.
  std::unique_ptr<WebAssociatedURLLoader> active_loader_;

  // When we encounter a redirect, this is the source of the redirect.
  GURL redirects_to_ ALLOW_DISCOURAGED_TYPE("Avoids conversion in media code");

  // If the server tries to gives us more bytes than we want, this how
  // many bytes we need to discard before we get to the right place.
  uint64_t bytes_to_discard_ = 0;

  // Is the client an audio element?
  bool is_client_audio_element_ = false;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<ResourceMultiBufferDataProvider> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_RESOURCE_MULTI_BUFFER_DATA_PROVIDER_H_
