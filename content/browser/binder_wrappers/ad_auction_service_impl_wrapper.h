// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BINDER_WRAPPERS_AD_AUCTION_SERVICE_IMPL_WRAPPER_H_
#define CONTENT_BROWSER_BINDER_WRAPPERS_AD_AUCTION_SERVICE_IMPL_WRAPPER_H_

#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {

class RenderFrameHost;

// These are only used from functions in internal namespace of
// browser_interface_binders.cc, so we use internal namespace here as well.
namespace internal {

void AdAuctionServiceImplCreateMojoService(
    mojo::BinderMapWithContext<RenderFrameHost*>& map);

}  // namespace internal

}  // namespace content

#endif  // CONTENT_BROWSER_BINDER_WRAPPERS_AD_AUCTION_SERVICE_IMPL_WRAPPER_H_
