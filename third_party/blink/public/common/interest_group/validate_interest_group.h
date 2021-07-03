// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_VALIDATE_INTEREST_GROUP_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_VALIDATE_INTEREST_GROUP_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"

namespace url {
class Origin;
}

namespace blink {

// Checks that the specified mojom::blink::InterestGroup can be added by
// `origin`. Returns false if it cannot be.

// Checks all provided URLs. Does no validation of expiration time. Does no
// validation of values expected to be in JSON, since it runs in the browser
// process. Only used outside of blink/, but provided by blink so can be tested
// against ValidateBlinkInterestGroup, which operates on
// mojom::blink::InterestGroups.
BLINK_COMMON_EXPORT bool ValidateInterestGroup(
    const url::Origin& origin,
    const mojom::InterestGroup& group);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_VALIDATE_INTEREST_GROUP_H_
