// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/url_loader_factory_bundle_mojom_traits.h"

#include <memory>
#include <utility>

#include "url/mojom/origin_mojom_traits.h"

namespace mojo {

using Traits = StructTraits<blink::mojom::URLLoaderFactoryBundleDataView,
                            std::unique_ptr<blink::URLLoaderFactoryBundleInfo>>;

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory> Traits::default_factory(
    BundleInfoType& bundle) {
  return std::move(bundle->pending_default_factory());
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory> Traits::appcache_factory(
    BundleInfoType& bundle) {
  return std::move(bundle->pending_appcache_factory());
}

// static
blink::URLLoaderFactoryBundleInfo::SchemeMap Traits::scheme_specific_factories(
    BundleInfoType& bundle) {
  return std::move(bundle->pending_scheme_specific_factories());
}

// static
blink::URLLoaderFactoryBundleInfo::OriginMap Traits::isolated_world_factories(
    BundleInfoType& bundle) {
  return std::move(bundle->pending_isolated_world_factories());
}

// static
bool Traits::bypass_redirect_checks(BundleInfoType& bundle) {
  return bundle->bypass_redirect_checks();
}

// static
bool Traits::Read(blink::mojom::URLLoaderFactoryBundleDataView data,
                  BundleInfoType* out_bundle) {
  *out_bundle = std::make_unique<blink::URLLoaderFactoryBundleInfo>();

  (*out_bundle)->pending_default_factory() = data.TakeDefaultFactory<
      mojo::PendingRemote<network::mojom::URLLoaderFactory>>();
  (*out_bundle)->pending_appcache_factory() = data.TakeAppcacheFactory<
      mojo::PendingRemote<network::mojom::URLLoaderFactory>>();
  if (!data.ReadSchemeSpecificFactories(
          &(*out_bundle)->pending_scheme_specific_factories())) {
    return false;
  }
  if (!data.ReadIsolatedWorldFactories(
          &(*out_bundle)->pending_isolated_world_factories())) {
    return false;
  }

  (*out_bundle)->set_bypass_redirect_checks(data.bypass_redirect_checks());

  return true;
}

}  // namespace mojo
