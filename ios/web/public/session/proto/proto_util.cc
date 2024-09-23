// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/session/proto/proto_util.h"

#include <ostream>
#include <type_traits>

#include "base/check.h"
#include "base/notreached.h"
#include "url/gurl.h"

namespace web {

base::Time TimeFromProto(const proto::Timestamp& storage) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(storage.microseconds()));
}

void SerializeTimeToProto(base::Time time, proto::Timestamp& storage) {
  storage.set_microseconds(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

UserAgentType UserAgentTypeFromProto(proto::UserAgentType value) {
  switch (value) {
    case proto::UserAgentType::None:
      return UserAgentType::NONE;

    case proto::UserAgentType::Automatic:
      return UserAgentType::AUTOMATIC;

    case proto::UserAgentType::Mobile:
      return UserAgentType::MOBILE;

    case proto::UserAgentType::Desktop:
      return UserAgentType::DESKTOP;

    default:
      NOTREACHED()
          << "Invalid web::proto::UserAgentType: "
          << static_cast<std::underlying_type<proto::UserAgentType>::type>(
                 value);
  }
}

proto::UserAgentType UserAgentTypeToProto(UserAgentType value) {
  switch (value) {
    case UserAgentType::NONE:
      return proto::UserAgentType::None;

    case UserAgentType::AUTOMATIC:
      return proto::UserAgentType::Automatic;

    case UserAgentType::MOBILE:
      return proto::UserAgentType::Mobile;

    case UserAgentType::DESKTOP:
      return proto::UserAgentType::Desktop;
  }

  NOTREACHED() << "Invalid web::UserAgentType: "
               << static_cast<std::underlying_type<UserAgentType>::type>(value);
}

ReferrerPolicy ReferrerPolicyFromProto(proto::ReferrerPolicy value) {
  switch (value) {
    case proto::ReferrerPolicy::Always:
      return ReferrerPolicyAlways;

    case proto::ReferrerPolicy::Default:
      return ReferrerPolicyDefault;

    case proto::ReferrerPolicy::NoReferrerWhenDowngrade:
      return ReferrerPolicyNoReferrerWhenDowngrade;

    case proto::ReferrerPolicy::Never:
      return ReferrerPolicyNever;

    case proto::ReferrerPolicy::Origin:
      return ReferrerPolicyOrigin;

    case proto::ReferrerPolicy::OriginWhenCrossOrigin:
      return ReferrerPolicyOriginWhenCrossOrigin;

    case proto::ReferrerPolicy::SameOrigin:
      return ReferrerPolicySameOrigin;

    case proto::ReferrerPolicy::StrictOrigin:
      return ReferrerPolicyStrictOrigin;

    case proto::ReferrerPolicy::StrictOriginWhenCrossOrigin:
      return ReferrerPolicyStrictOriginWhenCrossOrigin;

    default:
      NOTREACHED()
          << "Invalid web::proto::ReferrerPolicy: "
          << static_cast<std::underlying_type<proto::ReferrerPolicy>::type>(
                 value);
  }
}

proto::ReferrerPolicy ReferrerPolicyToProto(ReferrerPolicy value) {
  switch (value) {
    case ReferrerPolicyAlways:
      return proto::ReferrerPolicy::Always;

    case ReferrerPolicyDefault:
      return proto::ReferrerPolicy::Default;

    case ReferrerPolicyNoReferrerWhenDowngrade:
      return proto::ReferrerPolicy::NoReferrerWhenDowngrade;

    case ReferrerPolicyNever:
      return proto::ReferrerPolicy::Never;

    case ReferrerPolicyOrigin:
      return proto::ReferrerPolicy::Origin;

    case ReferrerPolicyOriginWhenCrossOrigin:
      return proto::ReferrerPolicy::OriginWhenCrossOrigin;

    case ReferrerPolicySameOrigin:
      return proto::ReferrerPolicy::SameOrigin;

    case ReferrerPolicyStrictOrigin:
      return proto::ReferrerPolicy::StrictOrigin;

    case ReferrerPolicyStrictOriginWhenCrossOrigin:
      return proto::ReferrerPolicy::StrictOriginWhenCrossOrigin;
  }

  NOTREACHED() << "Invalid web::ReferrerPolicy: "
               << static_cast<std::underlying_type<ReferrerPolicy>::type>(
                      value);
}

Referrer ReferrerFromProto(const proto::ReferrerStorage& storage) {
  return Referrer(GURL(storage.url()),
                  ReferrerPolicyFromProto(storage.policy()));
}

void SerializeReferrerToProto(const Referrer& referrer,
                              proto::ReferrerStorage& storage) {
  CHECK(referrer.url.is_valid());
  storage.set_url(referrer.url.spec());
  storage.set_policy(ReferrerPolicyToProto(referrer.policy));
}

}  // namespace web
