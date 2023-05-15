// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/navigation/referrer.h"

namespace web {

std::ostream& operator<<(std::ostream& stream, ReferrerPolicy policy) {
  switch (policy) {
    case ReferrerPolicyAlways:
      return stream << "ReferrerPolicyAlways";

    case ReferrerPolicyDefault:
      return stream << "ReferrerPolicyDefault";

    case ReferrerPolicyNoReferrerWhenDowngrade:
      return stream << "ReferrerPolicyNoReferrerWhenDowngrade";

    case ReferrerPolicyNever:
      return stream << "ReferrerPolicyNever";

    case ReferrerPolicyOrigin:
      return stream << "ReferrerPolicyOrigin";

    case ReferrerPolicyOriginWhenCrossOrigin:
      return stream << "ReferrerPolicyOriginWhenCrossOrigin";

    case ReferrerPolicySameOrigin:
      return stream << "ReferrerPolicySameOrigin";

    case ReferrerPolicyStrictOrigin:
      return stream << "ReferrerPolicyStrictOrigin";

    case ReferrerPolicyStrictOriginWhenCrossOrigin:
      return stream << "ReferrerPolicyStrictOriginWhenCrossOrigin";
  }
}

bool operator==(const Referrer& lhs, const Referrer& rhs) {
  return lhs.url == rhs.url && lhs.policy == rhs.policy;
}

bool operator!=(const Referrer& lhs, const Referrer& rhs) {
  return lhs.url != rhs.url || lhs.policy != rhs.policy;
}

std::ostream& operator<<(std::ostream& stream, const Referrer& referrer) {
  return stream << "Referrer{ url = \"" << referrer.url << "\", "
                << "policy = " << referrer.policy << " }";
}

}  // namespace web
