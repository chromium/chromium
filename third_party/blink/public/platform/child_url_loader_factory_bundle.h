// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CHILD_URL_LOADER_FACTORY_BUNDLE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CHILD_URL_LOADER_FACTORY_BUNDLE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// Holds the internal state of a ChildURLLoaderFactoryBundle in a form that is
// safe to pass across sequences.
//
// |pending_subresource_proxying_loader_factory| is used only by the frames who
// may send prefetch requests by <link rel="prefetch"> tags, or send fetch
// requests with {browsingTopics: true} flag. For prefetch, this loader factory
// allows prefetch loading to be done by the browser process (therefore less
// memory pressure), and also adds special handling for Signed Exchanges (SXG)
// when the flag is enabled. TODO(crbug/803776): deprecate this once SXG
// specific code is moved into Network Service unless we see huge memory benefit
// for doing this. For topics, this loader factory allows intercepting and
// processing the topics headers in the browser process.
//
// |pending_keep_alive_loader_factory| is used only by the frames who may send
// fetch requests with {keepalive: true} flag. The loader factory allows
// keepalive request handling to be proxied via the browser process. The browser
// may forward the response back if the request initiator frame is still alive.
// It is only set if `blink::features::kKeepAliveInBrowserMigration` is true.
// See also crbug.com/1356128 and
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY
class BLINK_PLATFORM_EXPORT ChildPendingURLLoaderFactoryBundle
    : public blink::PendingURLLoaderFactoryBundle {
 public:
  ChildPendingURLLoaderFactoryBundle();
  explicit ChildPendingURLLoaderFactoryBundle(
      std::unique_ptr<PendingURLLoaderFactoryBundle> base_factories);
  ChildPendingURLLoaderFactoryBundle(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_default_factory,
      SchemeMap pending_scheme_specific_factories,
      OriginMap pending_isolated_world_factories,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_subresource_proxying_loader_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_keep_alive_loader_factory,
      bool bypass_redirect_checks);
  ChildPendingURLLoaderFactoryBundle(
      const ChildPendingURLLoaderFactoryBundle&) = delete;
  ChildPendingURLLoaderFactoryBundle& operator=(
      const ChildPendingURLLoaderFactoryBundle&) = delete;
  ~ChildPendingURLLoaderFactoryBundle() override;

  static std::unique_ptr<ChildPendingURLLoaderFactoryBundle>
  CreateFromDefaultFactoryImpl(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_default_factory) {
    std::unique_ptr<ChildPendingURLLoaderFactoryBundle> pending_bundle(
        new ChildPendingURLLoaderFactoryBundle(
            std::move(pending_default_factory),
            {},       // pending_scheme_specific_factories
            {},       // pending_isolated_world_factories
            {},       // pending_subresource_proxying_loader_factory
            {},       // pending_keep_alive_loader_factory
            false));  // bypass_redirect_checks
    return pending_bundle;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>&
  pending_subresource_proxying_loader_factory() {
    return pending_subresource_proxying_loader_factory_;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>&
  pending_keep_alive_loader_factory() {
    return pending_keep_alive_loader_factory_;
  }

 protected:
  // PendingURLLoaderFactoryBundle overrides.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_subresource_proxying_loader_factory_;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_keep_alive_loader_factory_;
};

// This class extends URLLoaderFactoryBundle to support prefetch loader factory
// and subresource overrides (the latter to support MimeHandlerViewGuest).
class BLINK_PLATFORM_EXPORT ChildURLLoaderFactoryBundle
    : public blink::URLLoaderFactoryBundle {
 public:
  ChildURLLoaderFactoryBundle();

  explicit ChildURLLoaderFactoryBundle(
      std::unique_ptr<ChildPendingURLLoaderFactoryBundle> pending_factories);

  // URLLoaderFactoryBundle overrides.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

  std::unique_ptr<ChildPendingURLLoaderFactoryBundle> PassInterface();

  void Update(
      std::unique_ptr<ChildPendingURLLoaderFactoryBundle> pending_factories);
  void UpdateSubresourceOverrides(
      std::vector<blink::mojom::TransferrableURLLoaderPtr>*
          subresource_overrides);
  void SetSubresourceProxyingLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          subresource_proxying_loader_factory);
  void SetKeepAliveLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          keep_alive_loader_factory);

  virtual bool IsHostChildURLLoaderFactoryBundle() const;

 protected:
  ~ChildURLLoaderFactoryBundle() override;

 private:
  mojo::Remote<network::mojom::URLLoaderFactory>
      subresource_proxying_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> keep_alive_loader_factory_;

  std::map<GURL, mojom::TransferrableURLLoaderPtr> subresource_overrides_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CHILD_URL_LOADER_FACTORY_BUNDLE_H_
