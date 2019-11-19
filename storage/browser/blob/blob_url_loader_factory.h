// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_LOADER_FACTORY_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_LOADER_FACTORY_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"

namespace storage {

class BlobDataHandle;
class BlobStorageContext;

// URLLoaderFactory that can create loaders for exactly one url, loading the
// blob that was passed to its constructor. This factory keeps the blob alive.
// Self destroys when no more bindings exist.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  static void Create(
      mojo::PendingRemote<blink::mojom::Blob> blob,
      const GURL& blob_url,
      base::WeakPtr<BlobStorageContext> context,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);

  // Creates a factory for a BlobURLToken. The token is used to look up the blob
  // and blob URL in the (browser side) BlobStorageRegistry, to ensure you can't
  // use a blob URL to load the contents of an unrelated blob.
  static void Create(
      mojo::PendingRemote<blink::mojom::BlobURLToken> token,
      base::WeakPtr<BlobStorageContext> context,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);

  // URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  BlobURLLoaderFactory(
      std::unique_ptr<BlobDataHandle> handle,
      const GURL& blob_url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);
  ~BlobURLLoaderFactory() override;
  void OnConnectionError();

  std::unique_ptr<BlobDataHandle> handle_;
  GURL url_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  DISALLOW_COPY_AND_ASSIGN(BlobURLLoaderFactory);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_LOADER_FACTORY_H_
