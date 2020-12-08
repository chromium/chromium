// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_INITIATOR_LOCK_COMPATIBILITY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_INITIATOR_LOCK_COMPATIBILITY_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "url/origin.h"

namespace network {

namespace mojom {
class URLLoaderFactoryParams;
}  // namespace mojom

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "RequestInitiatorOriginLockCompatibility" in
// tools/metrics/histograms/enums.xml.
enum class InitiatorLockCompatibility {
  // Request came from a browser process and so the
  // |request_initiator_origin_lock| doesn't apply.
  kBrowserProcess = 0,

  // |request_initiator_origin_lock| is missing.  For historical context see
  // https://crbug.com/1098938.
  kNoLock = 1,

  // |request_initiator| is missing.  This indicates that the renderer has a bug
  // or has been compromised by an attacker.
  kNoInitiator = 2,

  // |request.request_initiator| is compatible with
  // |factory_params_.request_initiator_origin_lock| - either
  // |request.request_initiator| is opaque or it is equal to
  // |request_initiator_origin_lock|.
  kCompatibleLock = 3,

  // |request.request_initiator| is incompatible with
  // |factory_params_.request_initiator_origin_lock|.  Cases known so far where
  // this can occur:
  // - HTML Imports (see https://crbug.com/871827#c9).
  kIncorrectLock = 4,

  // Covered by AddAllowedRequestInitiatorForPlugin.
  kAllowedRequestInitiatorForPlugin = 7,

  kMaxValue = kAllowedRequestInitiatorForPlugin,
};

// Verifies if |request.request_initiator| matches
// |factory_params.request_initiator_origin_lock|.
//
// This should only be called for requests from renderer processes
// (ones that are not coverd by the kExcludedPlugin exception).
COMPONENT_EXPORT(NETWORK_CPP)
InitiatorLockCompatibility VerifyRequestInitiatorLock(
    const base::Optional<url::Origin>& request_initiator_origin_lock,
    const base::Optional<url::Origin>& request_initiator);

// Gets initiator of request, falling back to a unique origin if
// 1) |request_initiator| is missing or
// 2) |request_initiator| is incompatible with |request_initiator_origin_lock|.
//
// |request_initiator_origin_lock| is the origin to which the URLLoaderFactory
// of the request is locked in a trustworthy way.
//   Example:
//     URLLoaderFactoryParams::request_initiator_origin_lock
//     SubresourceSignedExchangeURLLoaderFactory::request_initiator_origin_lock
// |request_initiator| should come from net::URLRequest::initiator() or
// network::ResourceRequest::request_initiator which may be initially set in an
// untrustworthy process (eg: renderer process).
//
// TODO(lukasza): Remove this function if https://crrev.com/c/1661114 sticks
// (i.e. if ResourceRequest::request_initiator is sanitized and made trustworthy
// by CorsURLLoaderFactory::CreateLoaderAndStart and IsValidRequest). Once we
// remove this, this header can be moved to non-public directory.
COMPONENT_EXPORT(NETWORK_CPP)
url::Origin GetTrustworthyInitiator(
    const base::Optional<url::Origin>& request_initiator_origin_lock,
    const base::Optional<url::Origin>& request_initiator);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_INITIATOR_LOCK_COMPATIBILITY_H_
