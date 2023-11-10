// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_LOADER_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace storage {
class BlobDataHandle;

// This class handles a request for a blob:// url. It self-destructs (directly,
// or after passing ownership to MojoBlobReader at the end of the Start
// method) when it has finished responding.
// Note: some of this code is duplicated from BlobURLRequestJob.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobURLLoader
    : public MojoBlobReader::Delegate,
      public network::mojom::URLLoader {
 public:
  static void CreateAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      std::unique_ptr<BlobDataHandle> blob_handle);
  static void CreateAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      std::unique_ptr<BlobDataHandle> blob_handle);

  BlobURLLoader(const BlobURLLoader&) = delete;
  BlobURLLoader& operator=(const BlobURLLoader&) = delete;

  ~BlobURLLoader() override;

 private:
  BlobURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      std::unique_ptr<BlobDataHandle> blob_handle);
  BlobURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      std::unique_ptr<BlobDataHandle> blob_handle);

  void Start(const std::string& method, const net::HttpRequestHeaders& headers);

  // network::mojom::URLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_request_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_request_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  // MojoBlobReader::Delegate implementation:
  RequestSideData DidCalculateSize(uint64_t total_size,
                                   uint64_t content_size) override;
  void DidReadSideData(std::optional<mojo_base::BigBuffer> data) override;
  void OnComplete(net::Error error_code, uint64_t total_written_bytes) override;

  void HeadersCompleted(net::HttpStatusCode status_code,
                        uint64_t content_size,
                        std::optional<mojo_base::BigBuffer> metadata);

  mojo::Receiver<network::mojom::URLLoader> receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  bool byte_range_set_ = false;
  net::HttpByteRange byte_range_;

  uint64_t total_size_ = 0;
  bool sent_headers_ = false;

  std::unique_ptr<BlobDataHandle> blob_handle_;
  mojo::ScopedDataPipeProducerHandle response_body_producer_handle_;
  mojo::ScopedDataPipeConsumerHandle response_body_consumer_handle_;

  base::WeakPtrFactory<BlobURLLoader> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_LOADER_H_
