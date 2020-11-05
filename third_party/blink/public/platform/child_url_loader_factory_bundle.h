// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CHILD_URL_LOADER_FACTORY_BUNDLE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CHILD_URL_LOADER_FACTORY_BUNDLE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
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
// |pending_prefetch_loader_factory| is used only by the frames who may send
// prefetch requests by <link rel="prefetch"> tags. The loader factory allows
// prefetch loading to be done by the browser process (therefore less memory
// pressure), and also adds special handling for Signed Exchanges (SXG) when the
// flag is enabled. TODO(crbug/803776): deprecate this once SXG specific code is
// moved into Network Service unless we see huge memory benefit for doing this.
class BLINK_PLATFORM_EXPORT ChildPendingURLLoaderFactoryBundle
    : public blink::PendingURLLoaderFactoryBundle {
 public:
  ChildPendingURLLoaderFactoryBundle();
  explicit ChildPendingURLLoaderFactoryBundle(
      std::unique_ptr<PendingURLLoaderFactoryBundle> base_factories);
  ChildPendingURLLoaderFactoryBundle(
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
  ~ChildPendingURLLoaderFactoryBundle() override;

  template <typename T>
  static std::unique_ptr<ChildPendingURLLoaderFactoryBundle>
  CreateFromDefaultFactoryImpl(std::unique_ptr<T> default_factory_impl) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
    mojo::MakeSelfOwnedReceiver(
        std::move(default_factory_impl),
        pending_remote.InitWithNewPipeAndPassReceiver());

    std::unique_ptr<ChildPendingURLLoaderFactoryBundle> pending_bundle(
        new ChildPendingURLLoaderFactoryBundle(
            std::move(pending_remote),  // pending_default_factory
            {},                         // pending_default_network_factory
            {},                         // pending_scheme_specific_factories
            {},                         // pending_isolated_world_factories
            {},                         // direct_network_factory_remote
            {},                         // pending_prefetch_loader_factory
            false));                    // bypass_redirect_checks
    return pending_bundle;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>&
  direct_network_factory_remote() {
    return direct_network_factory_remote_;
  }
  mojo::PendingRemote<network::mojom::URLLoaderFactory>&
  pending_prefetch_loader_factory() {
    return pending_prefetch_loader_factory_;
  }

  void MarkAsDeprecatedProcessWideFactory() {
    is_deprecated_process_wide_factory_ = true;
  }
  bool is_deprecated_process_wide_factory() const {
    return is_deprecated_process_wide_factory_;
  }

 protected:
  // PendingURLLoaderFactoryBundle overrides.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  bool is_deprecated_process_wide_factory_ = false;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      direct_network_factory_remote_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_prefetch_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildPendingURLLoaderFactoryBundle);
};

// This class extends URLLoaderFactoryBundle to support a direct network loader
// factory, which bypasses custom overrides such as appcache or service worker.
// Besides, it also supports using callbacks to lazily initialize the direct
// network loader factory.
class BLINK_PLATFORM_EXPORT ChildURLLoaderFactoryBundle
    : public blink::URLLoaderFactoryBundle {
 public:
  using FactoryGetterCallback = base::OnceCallback<
      mojo::PendingRemote<network::mojom::URLLoaderFactory>()>;

  ChildURLLoaderFactoryBundle();

  explicit ChildURLLoaderFactoryBundle(
      std::unique_ptr<ChildPendingURLLoaderFactoryBundle> pending_factories);

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
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

  // Does the same as Clone(), but without cloning the appcache_factory_.
  // This is used for creating a bundle for network fallback loading with
  // Service Workers (where AppCache must be skipped), and only when
  // claim() is called.
  virtual std::unique_ptr<network::PendingSharedURLLoaderFactory>
  CloneWithoutAppCacheFactory();

  std::unique_ptr<ChildPendingURLLoaderFactoryBundle> PassInterface();

  void Update(
      std::unique_ptr<ChildPendingURLLoaderFactoryBundle> pending_factories);
  void UpdateSubresourceOverrides(
      std::vector<blink::mojom::TransferrableURLLoaderPtr>*
          subresource_overrides);
  void SetPrefetchLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory);

  virtual bool IsHostChildURLLoaderFactoryBundle() const;

  void MarkAsDeprecatedProcessWideFactory() {
    is_deprecated_process_wide_factory_ = true;
  }

 protected:
  ~ChildURLLoaderFactoryBundle() override;

  // URLLoaderFactoryBundle overrides.
  network::mojom::URLLoaderFactory* GetFactory(
      const network::ResourceRequest& request) override;

 private:
  void InitDirectNetworkFactoryIfNecessary();
  std::unique_ptr<network::PendingSharedURLLoaderFactory> CloneInternal(
      bool include_appcache);

  FactoryGetterCallback direct_network_factory_getter_;
  mojo::Remote<network::mojom::URLLoaderFactory> direct_network_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> prefetch_loader_factory_;

  bool is_deprecated_process_wide_factory_ = false;

  std::map<GURL, mojom::TransferrableURLLoaderPtr> subresource_overrides_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_CHILD_URL_LOADER_FACTORY_BUNDLE_H_
