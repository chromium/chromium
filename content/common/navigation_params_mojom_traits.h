// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_MOJOM_TRAITS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/clone_traits.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/navigation/prefetched_signed_exchange_info.mojom.h"

namespace mojo {

template <>
struct CloneTraits<blink::mojom::PrefetchedSignedExchangeInfoPtr, true> {
  static blink::mojom::PrefetchedSignedExchangeInfoPtr Clone(
      const blink::mojom::PrefetchedSignedExchangeInfoPtr& input) {
    return blink::mojom::PrefetchedSignedExchangeInfo::New(
        mojo::Clone(input->outer_url), mojo::Clone(input->header_integrity),
        mojo::Clone(input->inner_url), mojo::Clone(input->inner_response),
        mojo::PendingRemote<network::mojom::URLLoaderFactory>());
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_MOJOM_TRAITS_H_
