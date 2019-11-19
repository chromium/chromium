// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_SYNC_LOAD_RESPONSE_H_
#define CONTENT_RENDERER_LOADER_SYNC_LOAD_RESPONSE_H_

#include <string>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "url/gurl.h"

namespace content {

class SyncLoadContext;

// See the SyncLoad method. (The name of this struct is not
// suffixed with "Info" because it also contains the response data.)
struct CONTENT_EXPORT SyncLoadResponse {
  SyncLoadResponse();
  SyncLoadResponse(SyncLoadResponse&& other);
  ~SyncLoadResponse();

  SyncLoadResponse& operator=(SyncLoadResponse&& other);

  base::Optional<net::RedirectInfo> redirect_info;
  SyncLoadContext* context_for_redirect = nullptr;

  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();

  // The response error code.
  int error_code;

  // The response extended error code.
  int extended_error_code = 0;

  // Optional CORS error details.
  base::Optional<network::CorsErrorStatus> cors_error;

  // The final URL of the response.  This may differ from the request URL in
  // the case of a server redirect.
  GURL url;

  // The response data.
  std::string data;

  // Used for blob response type XMLHttpRequest.
  blink::mojom::SerializedBlobPtr downloaded_blob;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_SYNC_LOAD_RESPONSE_H_
