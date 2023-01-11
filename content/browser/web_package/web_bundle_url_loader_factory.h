// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_URL_LOADER_FACTORY_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class WebBundleReader;

// A class to implements network::mojom::URLLoaderFactory that supports
// WebBundle.
class CONTENT_EXPORT WebBundleURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  explicit WebBundleURLLoaderFactory(scoped_refptr<WebBundleReader> reader,
                                     int frame_tree_node_id);

  WebBundleURLLoaderFactory(const WebBundleURLLoaderFactory&) = delete;
  WebBundleURLLoaderFactory& operator=(const WebBundleURLLoaderFactory&) =
      delete;

  ~WebBundleURLLoaderFactory() override;

  // Set a |network::mojom::URLLoaderFactory| remote interface used for requests
  // that are not found in the WebBundle. This will override the existing
  // fallback_factory if it was set previously.
  void SetFallbackFactory(
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory);

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  const scoped_refptr<WebBundleReader>& reader() const { return reader_; }

 private:
  class EntryLoader;

  bool CanHandleRequest(const network::ResourceRequest& resource_request) const;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  scoped_refptr<WebBundleReader> reader_;
  const int frame_tree_node_id_;
  mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory_;

  base::WeakPtrFactory<WebBundleURLLoaderFactory> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_URL_LOADER_FACTORY_H_
