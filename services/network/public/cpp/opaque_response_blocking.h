// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_OPAQUE_RESPONSE_BLOCKING_H_
#define SERVICES_NETWORK_PUBLIC_CPP_OPAQUE_RESPONSE_BLOCKING_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

// Result of applying heuristics based on partial ORB algorithm (suitable only
// for UMA).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ResponseHeadersHeuristicForUma {
  // ORB only applies to opaque respones.  The enum value below includes:
  // - mode != no-cors (e.g. mode=navigate or mode=cors)
  // - browser-initiated requests
  kNonOpaqueResponse = 0,

  // ORB algorithm can *surely* make a decision based on response headers.  The
  // enum value below covers all subresource requests:
  // - scripts, images, video, etc.
  // - both same-origin and cross-origin requests
  kProcessedBasedOnHeaders = 1,

  // ORB algorithm *might* require parsing the response body as Javascript.
  //
  // This might be a false positive if the response:
  // 1) sniffs as an audio/image/video format
  // 2) represents a valid range response for a media element
  kRequiresJavascriptParsing = 2,

  // Required enum value to support UMA macros.
  kMaxValue = kRequiresJavascriptParsing,
};

// Logs UMA tracking expected behavior characteristics of the Opaque Response
// Blocking algorithm (a potential successor of CORB aka Cross-Origin Read
// Blocking).  See also https://github.com/annevk/orb
COMPONENT_EXPORT(NETWORK_CPP)
void LogUmaForOpaqueResponseBlocking(
    const GURL& request_url,
    const base::Optional<url::Origin>& request_initiator,
    mojom::RequestMode request_mode,
    mojom::RequestDestination request_destination,
    const mojom::URLResponseHead& response);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_OPAQUE_RESPONSE_BLOCKING_H_
