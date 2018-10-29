// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_STRUCT_TRAITS_H_
#define CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_STRUCT_TRAITS_H_

#include <memory>

#include "content/common/url_loader_factory_bundle.h"
#include "content/common/url_loader_factory_bundle.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::URLLoaderFactoryBundleDataView,
                    std::unique_ptr<content::URLLoaderFactoryBundleInfo>> {
  using BundleInfoType = std::unique_ptr<content::URLLoaderFactoryBundleInfo>;

  static bool IsNull(const BundleInfoType& bundle) { return !bundle; }

  static void SetToNull(BundleInfoType* bundle) { bundle->reset(); }

  static network::mojom::URLLoaderFactoryPtrInfo default_factory(
      BundleInfoType& bundle);

  static content::URLLoaderFactoryBundleInfo::SchemeMap
  scheme_specific_factories(BundleInfoType& bundle);

  static content::URLLoaderFactoryBundleInfo::OriginMap
  initiator_specific_factories(BundleInfoType& bundle);

  static bool bypass_redirect_checks(BundleInfoType& bundle);

  static bool Read(content::mojom::URLLoaderFactoryBundleDataView data,
                   BundleInfoType* out_bundle);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_STRUCT_TRAITS_H_
