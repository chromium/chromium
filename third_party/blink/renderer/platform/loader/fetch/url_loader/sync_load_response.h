// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_SYNC_LOAD_RESPONSE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_SYNC_LOAD_RESPONSE_H_

#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_data.h"
#include "url/gurl.h"

namespace blink {

// See the SyncLoad method. (The name of this struct is not
// suffixed with "Info" because it also contains the response data.)
struct BLINK_PLATFORM_EXPORT SyncLoadResponse {
  SyncLoadResponse();
  SyncLoadResponse(SyncLoadResponse&& other);
  ~SyncLoadResponse();

  SyncLoadResponse& operator=(SyncLoadResponse&& other);

  absl::optional<net::RedirectInfo> redirect_info;

  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();

  // The response error code.
  int error_code;

  // The response extended error code.
  int extended_error_code = 0;

  bool should_collapse_initiator = false;

  // Detailed host resolution error information.
  net::ResolveErrorInfo resolve_error_info;

  // Optional CORS error details.
  absl::optional<network::CorsErrorStatus> cors_error;

  // The final URL of the response.  This may differ from the request URL in
  // the case of a server redirect.
  // Use GURL to avoid extra conversion between KURL and GURL because non-blink
  // types are allowed for loader here.
  GURL url;

  // The response data.
  WebData data;

  // Used for blob response type XMLHttpRequest.
  mojom::SerializedBlobPtr downloaded_blob;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_SYNC_LOAD_RESPONSE_H_
