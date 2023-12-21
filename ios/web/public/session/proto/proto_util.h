// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_UTIL_H_
#define IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_UTIL_H_

#include "base/time/time.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/session/proto/common.pb.h"
#include "ios/web/public/session/proto/navigation.pb.h"

namespace web {

// Creates a base::Time value from serialized `storage`.
base::Time TimeFromProto(const proto::Timestamp& storage);

// Serializes `time` into `storage`.
void SerializeTimeToProto(base::Time time, proto::Timestamp& storage);

// Converts a web::proto::UserAgentType and web::UserAgentType.
UserAgentType UserAgentTypeFromProto(proto::UserAgentType value);

// Converts a web::UserAgentType and web::proto::UserAgentType.
proto::UserAgentType UserAgentTypeToProto(UserAgentType value);

// Converts a web::proto::ReferrerPolicy to a web::ReferrerPolicy.
ReferrerPolicy ReferrerPolicyFromProto(proto::ReferrerPolicy value);

// Converts a web::ReferrerPolicy to a web::proto::ReferrerPolicy.
proto::ReferrerPolicy ReferrerPolicyToProto(ReferrerPolicy value);

// Creates a Referrer from serialized `storage`.
Referrer ReferrerFromProto(const proto::ReferrerStorage& storage);

// Serializes `referrer` into `storage`.
void SerializeReferrerToProto(const Referrer& referrer,
                              proto::ReferrerStorage& storage);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_UTIL_H_
