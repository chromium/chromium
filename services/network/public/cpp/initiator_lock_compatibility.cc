// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/initiator_lock_compatibility.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/types/optional_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

namespace {

base::debug::CrashKeyString* GetRequestInitiatorOriginLockCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_initiator_origin_lock", base::debug::CrashKeySize::Size64);
  return crash_key;
}

}  // namespace

InitiatorLockCompatibility VerifyRequestInitiatorLock(
    const std::optional<url::Origin>& request_initiator_origin_lock,
    const std::optional<url::Origin>& request_initiator) {
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

namespace debug {

ScopedRequestInitiatorOriginLockCrashKey::
    ScopedRequestInitiatorOriginLockCrashKey(
        const std::optional<url::Origin>& request_initiator_origin_lock)
    : ScopedOriginCrashKey(GetRequestInitiatorOriginLockCrashKey(),
                           base::OptionalToPtr(request_initiator_origin_lock)) {
}

ScopedRequestInitiatorOriginLockCrashKey::
    ~ScopedRequestInitiatorOriginLockCrashKey() = default;

}  // namespace debug
}  // namespace network
