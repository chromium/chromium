// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_MOJOM_TRAITS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_MOJOM_TRAITS_H_

#include "content/common/prefetched_signed_exchange_info.mojom.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace mojo {

template <>
struct CloneTraits<content::mojom::PrefetchedSignedExchangeInfoPtr, true> {
  static content::mojom::PrefetchedSignedExchangeInfoPtr Clone(
      const content::mojom::PrefetchedSignedExchangeInfoPtr& input) {
    return content::mojom::PrefetchedSignedExchangeInfo::New(
        mojo::Clone(input->outer_url), mojo::Clone(input->header_integrity),
        mojo::Clone(input->inner_url), mojo::Clone(input->inner_response),
        mojo::PendingRemote<network::mojom::URLLoaderFactory>());
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_MOJOM_TRAITS_H_
