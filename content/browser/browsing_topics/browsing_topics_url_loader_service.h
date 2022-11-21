// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_SERVICE_H_
#define CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"

namespace content {

// A URLLoaderFactory that can be passed to a renderer to use for performing
// topics related requests. The renderer uses this to handle
// fetch(<url>, {browsingTopics: true}).
class CONTENT_EXPORT BrowsingTopicsURLLoaderService final
    : public network::mojom::URLLoaderFactory {
 public:
  struct CONTENT_EXPORT BindContext {
    // `factory` is a clone of the default factory bundle for document
    // subresource requests.
    explicit BindContext(
        scoped_refptr<network::SharedURLLoaderFactory> factory);

    explicit BindContext(const std::unique_ptr<BindContext>& other);

    ~BindContext();

    // Set `document` to `committed_document`.
    void OnDidCommitNavigation(WeakDocumentPtr committed_document);

    // Upon NavigationRequest::DidCommitNavigation(), `document` will be set to
    // the document that this `BindContext` is associated with. It will become
    // null whenever the document navigates away.
    WeakDocumentPtr document;

    // The factory to use for the requests initiated from this context.
    scoped_refptr<network::SharedURLLoaderFactory> factory;

    // This must be the last member.
    base::WeakPtrFactory<BrowsingTopicsURLLoaderService::BindContext>
        weak_ptr_factory{this};
  };

  BrowsingTopicsURLLoaderService();

  ~BrowsingTopicsURLLoaderService() override;

  BrowsingTopicsURLLoaderService(const BrowsingTopicsURLLoaderService&) =
      delete;
  BrowsingTopicsURLLoaderService& operator=(
      const BrowsingTopicsURLLoaderService&) = delete;

  // Binds `receiver`. Creates a `BindContext` to contain a factory constructed
  // with `pending_factory`, and associates it to `receiver`. Returns the
  // associated `BindContext`.
  base::WeakPtr<BindContext> GetFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory);

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                    std::unique_ptr<BindContext>>
      loader_factory_receivers_;

  mojo::ReceiverSet<network::mojom::URLLoader,
                    std::unique_ptr<network::mojom::URLLoader>>
      loader_receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_URL_LOADER_SERVICE_H_
