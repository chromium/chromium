// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_CHILD_URL_LOADER_FACTORY_BUNDLE_H_
#define CONTENT_RENDERER_LOADER_CHILD_URL_LOADER_FACTORY_BUNDLE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"

namespace content {

// Holds the internal state of a ChildURLLoaderFactoryBundle in a form that is
// safe to pass across sequences.
// |pending_prefetch_loader_factory| is used only by the frames who may send
// prefetch requests by <link rel="prefetch"> tags. The loader factory allows
// prefetch loading to be done by the browser process (therefore less memory
// pressure), and also adds special handling for Signed Exchanges (SXG) when the
// flag is enabled. TODO(crbug/803776): deprecate this once SXG specific code is
// moved into Network Service unless we see huge memory benefit for doing this.
// TODO(domfarolino, crbug.com/955171): This class should be renamed to not
// include "Info".
class CONTENT_EXPORT ChildURLLoaderFactoryBundleInfo
    : public blink::URLLoaderFactoryBundleInfo {
 public:
  ChildURLLoaderFactoryBundleInfo();
  explicit ChildURLLoaderFactoryBundleInfo(
      std::unique_ptr<URLLoaderFactoryBundleInfo> base_factories);
  ChildURLLoaderFactoryBundleInfo(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_default_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_default_network_factory,
      SchemeMap pending_scheme_specific_factories,
      OriginMap pending_isolated_world_factories,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          direct_network_factory_remote,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_prefetch_loader_factory,
      bool bypass_redirect_checks);
  ~ChildURLLoaderFactoryBundleInfo() override;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>&
  direct_network_factory_remote() {
    return direct_network_factory_remote_;
  }
  mojo::PendingRemote<network::mojom::URLLoaderFactory>&
  pending_prefetch_loader_factory() {
    return pending_prefetch_loader_factory_;
  }

 protected:
  // URLLoaderFactoryBundleInfo overrides.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      direct_network_factory_remote_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_prefetch_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildURLLoaderFactoryBundleInfo);
};

// This class extends URLLoaderFactoryBundle to support a direct network loader
// factory, which bypasses custom overrides such as appcache or service worker.
// Besides, it also supports using callbacks to lazily initialize the direct
// network loader factory.
class CONTENT_EXPORT ChildURLLoaderFactoryBundle
    : public blink::URLLoaderFactoryBundle {
 public:
  using FactoryGetterCallback = base::OnceCallback<
      mojo::PendingRemote<network::mojom::URLLoaderFactory>()>;

  ChildURLLoaderFactoryBundle();

  explicit ChildURLLoaderFactoryBundle(
      std::unique_ptr<ChildURLLoaderFactoryBundleInfo> pending_factories);

  ChildURLLoaderFactoryBundle(
      FactoryGetterCallback direct_network_factory_getter);

  // URLLoaderFactoryBundle overrides.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override;

  // Does the same as Clone(), but without cloning the appcache_factory_.
  // This is used for creating a bundle for network fallback loading with
  // Service Workers (where AppCache must be skipped), and only when
  // claim() is called.
  virtual std::unique_ptr<network::SharedURLLoaderFactoryInfo>
  CloneWithoutAppCacheFactory();

  std::unique_ptr<ChildURLLoaderFactoryBundleInfo> PassInterface();

  void Update(
      std::unique_ptr<ChildURLLoaderFactoryBundleInfo> pending_factories);
  void UpdateSubresourceOverrides(
      std::vector<mojom::TransferrableURLLoaderPtr>* subresource_overrides);
  void SetPrefetchLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory);

  virtual bool IsHostChildURLLoaderFactoryBundle() const;

 protected:
  ~ChildURLLoaderFactoryBundle() override;

  // URLLoaderFactoryBundle overrides.
  network::mojom::URLLoaderFactory* GetFactory(
      const network::ResourceRequest& request) override;

 private:
  void InitDirectNetworkFactoryIfNecessary();
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> CloneInternal(
      bool include_appcache);

  FactoryGetterCallback direct_network_factory_getter_;
  mojo::Remote<network::mojom::URLLoaderFactory> direct_network_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> prefetch_loader_factory_;

  std::map<GURL, mojom::TransferrableURLLoaderPtr> subresource_overrides_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_CHILD_URL_LOADER_FACTORY_BUNDLE_H_
