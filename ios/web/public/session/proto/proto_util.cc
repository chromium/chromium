// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/session/proto/proto_util.h"

#include <ostream>
#include <type_traits>

#include "base/notreached.h"

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
      NOTREACHED_NORETURN()
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

  NOTREACHED_NORETURN()
      << "Invalid web::UserAgentType: "
      << static_cast<std::underlying_type<UserAgentType>::type>(value);
}

}  // namespace web
