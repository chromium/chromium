// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_messaging_utils.h"

#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-blink.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

String PushRegistrationStatusToString(mojom::PushRegistrationStatus status) {
  switch (status) {
    case mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE:
    case mojom::PushRegistrationStatus::
        SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE:
      return "Registration successful - from push service";

    case mojom::PushRegistrationStatus::NO_SERVICE_WORKER:
      return "Registration failed - no Service Worker";

    case mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE:
      return "Registration failed - push service not available";

    case mojom::PushRegistrationStatus::LIMIT_REACHED:
      return "Registration failed - registration limit has been reached";

    case mojom::PushRegistrationStatus::PERMISSION_DENIED:
      return "Registration failed - permission denied";

    case mojom::PushRegistrationStatus::SERVICE_ERROR:
      return "Registration failed - push service error";

    case mojom::PushRegistrationStatus::NO_SENDER_ID:
      return "Registration failed - missing applicationServerKey, and "
             "gcm_sender_id not found in manifest";

    case mojom::PushRegistrationStatus::STORAGE_ERROR:
      return "Registration failed - storage error";

    case mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE:
      return "Registration successful - from cache";

    case mojom::PushRegistrationStatus::NETWORK_ERROR:
      return "Registration failed - could not connect to push server";

    case mojom::PushRegistrationStatus::INCOGNITO_PERMISSION_DENIED:
      // We split this out for UMA, but it must be indistinguishable to JS.
      return PushRegistrationStatusToString(
          mojom::PushRegistrationStatus::PERMISSION_DENIED);

    case mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE:
      return "Registration failed - could not retrieve the public key";

    case mojom::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING:
      return "Registration failed - missing applicationServerKey, and manifest "
             "empty or missing";

    case mojom::PushRegistrationStatus::SENDER_ID_MISMATCH:
      return "Registration failed - A subscription with a different "
             "applicationServerKey (or gcm_sender_id) already exists; to "
             "change the applicationServerKey, unsubscribe then resubscribe.";

    case mojom::PushRegistrationStatus::STORAGE_CORRUPT:
      return "Registration failed - storage corrupt";

    case mojom::PushRegistrationStatus::RENDERER_SHUTDOWN:
      return "Registration failed - renderer shutdown";

    case mojom::PushRegistrationStatus::UNSUPPORTED_GCM_SENDER_ID:
      return "Registration failed - GCM Sender IDs are no longer supported, "
             "please upgrade to VAPID authentication instead";
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

mojom::PushErrorType PushRegistrationStatusToPushErrorType(
    mojom::PushRegistrationStatus status) {
  mojom::PushErrorType error_type = mojom::PushErrorType::ABORT;
  switch (status) {
    case mojom::PushRegistrationStatus::PERMISSION_DENIED:
      error_type = mojom::PushErrorType::NOT_ALLOWED;
      break;
    case mojom::PushRegistrationStatus::SENDER_ID_MISMATCH:
      error_type = mojom::PushErrorType::INVALID_STATE;
      break;
    case mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE:
    case mojom::PushRegistrationStatus::
        SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE:
    case mojom::PushRegistrationStatus::NO_SERVICE_WORKER:
    case mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE:
    case mojom::PushRegistrationStatus::LIMIT_REACHED:
    case mojom::PushRegistrationStatus::SERVICE_ERROR:
    case mojom::PushRegistrationStatus::NO_SENDER_ID:
    case mojom::PushRegistrationStatus::STORAGE_ERROR:
    case mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE:
    case mojom::PushRegistrationStatus::NETWORK_ERROR:
    case mojom::PushRegistrationStatus::INCOGNITO_PERMISSION_DENIED:
    case mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE:
    case mojom::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING:
    case mojom::PushRegistrationStatus::STORAGE_CORRUPT:
    case mojom::PushRegistrationStatus::RENDERER_SHUTDOWN:
    case mojom::PushRegistrationStatus::UNSUPPORTED_GCM_SENDER_ID:
      error_type = mojom::PushErrorType::ABORT;
      break;
  }
  return error_type;
}

}  // namespace blink
