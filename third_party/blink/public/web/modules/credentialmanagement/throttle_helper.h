// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_CREDENTIALMANAGEMENT_THROTTLE_HELPER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_CREDENTIALMANAGEMENT_THROTTLE_HELPER_H_

#include "third_party/blink/public/platform/web_common.h"

namespace url {
class Origin;
}

namespace blink {

namespace mojom {
enum class IdpSigninStatus;
}  // namespace mojom

class WebLocalFrame;

// Sets the identity provider (IDP) signin state of the given |origin| to
// |status|. This is meant for use with IdentityUrlLoaderThrottle.
BLINK_MODULES_EXPORT void SetIdpSigninStatus(WebLocalFrame* frame,
                                             const url::Origin& origin,
                                             mojom::IdpSigninStatus status);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_CREDENTIALMANAGEMENT_THROTTLE_HELPER_H_
