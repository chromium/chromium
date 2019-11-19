// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_RESOURCE_MULTIBUFFER_DATA_PROVIDER_H_
#define MEDIA_BLINK_RESOURCE_MULTIBUFFER_DATA_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/blink/media_blink_export.h"
#include "media/blink/multibuffer.h"
#include "media/blink/url_index.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_frame.h"
#include "url/gurl.h"

namespace blink {
class WebAssociatedURLLoader;
}  // namespace blink

namespace media {

class MEDIA_BLINK_EXPORT ResourceMultiBufferDataProvider
    : public MultiBuffer::DataProvider,
      public blink::WebAssociatedURLLoaderClient {
 public:
  // NUmber of times we'll retry if the connection fails.
  enum { kMaxRetries = 30 };

  ResourceMultiBufferDataProvider(UrlData* url_data,
                                  MultiBufferBlockId pos,
                                  bool is_client_audio_element);
  ~ResourceMultiBufferDataProvider() override;

  // Virtual for testing purposes.
  virtual void Start();

  // MultiBuffer::DataProvider implementation
  MultiBufferBlockId Tell() const override;
  bool Available() const override;
  int64_t AvailableBytes() const override;
  scoped_refptr<DataBuffer> Read() override;
  void SetDeferred(bool defer) override;

  // blink::WebAssociatedURLLoaderClient implementation.
  bool WillFollowRedirect(
      const blink::WebURL& new_url,
      const blink::WebURLResponse& redirect_response) override;
  void DidSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidDownloadData(uint64_t data_length) override;
  void DidReceiveData(const char* data, int data_length) override;
  void DidReceiveCachedMetadata(const char* data, int dataLength) override;
  void DidFinishLoading() override;
  void DidFail(const blink::WebURLError&) override;

  // Use protected instead of private for testing purposes.
 protected:
  friend class MultibufferDataSourceTest;
  friend class ResourceMultiBufferDataProviderTest;
  friend class MockBufferedDataSource;

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
  bool VerifyPartialResponse(const blink::WebURLResponse& response,
                             const scoped_refptr<UrlData>& url_data);

  // Current Position.
  MultiBufferBlockId pos_;

  // This is where we actually get read data from.
  // We don't need (or want) a scoped_refptr for this one, because
  // we are owned by it. Note that we may change this when we encounter
  // a redirect because we actually change ownership.
  UrlData* url_data_;

  // Temporary storage for incoming data.
  std::list<scoped_refptr<DataBuffer>> fifo_;

  // How many retries have we done at the current position.
  int retries_;

  // Copy of url_data_->cors_mode()
  // const to make it obvious that redirects cannot change it.
  const UrlData::CorsMode cors_mode_;

  // The origin for the initial request.
  // const to make it obvious that redirects cannot change it.
  const GURL origin_;

  // Keeps track of an active WebAssociatedURLLoader.
  // Only valid while loading resource.
  std::unique_ptr<blink::WebAssociatedURLLoader> active_loader_;

  // When we encounter a redirect, this is the source of the redirect.
  GURL redirects_to_;

  // If the server tries to gives us more bytes than we want, this how
  // many bytes we need to discard before we get to the right place.
  uint64_t bytes_to_discard_ = 0;

  // Is the client an audio element?
  bool is_client_audio_element_ = false;

  base::WeakPtrFactory<ResourceMultiBufferDataProvider> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BLINK_RESOURCE_MULTIBUFFER_DATA_PROVIDER_H_
