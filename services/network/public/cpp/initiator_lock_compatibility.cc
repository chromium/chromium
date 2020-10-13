// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/initiator_lock_compatibility.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

InitiatorLockCompatibility VerifyRequestInitiatorLock(
    const base::Optional<url::Origin>& request_initiator_origin_lock,
    const base::Optional<url::Origin>& request_initiator) {
  if (!request_initiator_origin_lock.has_value())
    return InitiatorLockCompatibility::kNoLock;
  const url::Origin& lock = request_initiator_origin_lock.value();

  if (!request_initiator.has_value())
    return InitiatorLockCompatibility::kNoInitiator;
  const url::Origin& initiator = request_initiator.value();

  if (initiator == lock)
    return InitiatorLockCompatibility::kCompatibleLock;

  // Opaque |initiator| is always allowed.  In particular, a factory locked to a
  // non-opaque |lock| may be used by an opaque |initiator| - for example when
  // the factory is inherited by a data: URL frame.
  if (initiator.opaque()) {
    // TODO(lukasza, nasko): Also consider equality of precursor origins (e.g.
    // if |initiator| is opaque, then it's precursor origin should match the
    // |lock| [or |lock|'s precursor if |lock| is also opaque]).
    return InitiatorLockCompatibility::kCompatibleLock;
  }

  return InitiatorLockCompatibility::kIncorrectLock;
}

url::Origin GetTrustworthyInitiator(
    const base::Optional<url::Origin>& request_initiator_origin_lock,
    const base::Optional<url::Origin>& request_initiator) {
  // Returning a unique origin as a fallback should be safe - such origin will
  // be considered cross-origin from all other origins.
  url::Origin unique_origin_fallback;

  if (!request_initiator.has_value())
    return unique_origin_fallback;

  InitiatorLockCompatibility initiator_compatibility =
      VerifyRequestInitiatorLock(request_initiator_origin_lock,
                                 request_initiator);
  if (initiator_compatibility == InitiatorLockCompatibility::kIncorrectLock)
    return unique_origin_fallback;

  // If all the checks above passed, then |request_initiator| is trustworthy.
  return request_initiator.value();
}

}  // namespace network
