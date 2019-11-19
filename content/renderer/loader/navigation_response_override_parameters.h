// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_NAVIGATION_RESPONSE_OVERRIDE_PARAMETERS_H_
#define CONTENT_RENDERER_LOADER_NAVIGATION_RESPONSE_OVERRIDE_PARAMETERS_H_

#include <vector>

#include "content/common/content_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace content {

// Used to override parameters of the navigation request.
struct CONTENT_EXPORT NavigationResponseOverrideParameters {
 public:
  NavigationResponseOverrideParameters();
  ~NavigationResponseOverrideParameters();

  std::vector<GURL> redirects;
  std::vector<network::mojom::URLResponseHeadPtr> redirect_responses;
  std::vector<net::RedirectInfo> redirect_infos;
  network::mojom::URLResponseHeadPtr response_head =
      network::mojom::URLResponseHead::New();
  mojo::ScopedDataPipeConsumerHandle response_body;
  network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_NAVIGATION_RESPONSE_OVERRIDE_PARAMETERS_H_
