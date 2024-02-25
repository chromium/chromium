// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/session/proto/proto_util.h"

#include "base/time/time.h"
#include "ios/web/public/session/proto/common.pb.h"
#include "ios/web/public/session/proto/navigation.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {

using SessionProtoUtilTest = PlatformTest;

// Tests that base::Time round trip correctly when serialized to proto.
TEST_F(SessionProtoUtilTest, TimeRoundTrip) {
  const base::Time time = base::Time::Now();

  proto::Timestamp storage;
  SerializeTimeToProto(time, storage);

  EXPECT_EQ(time, TimeFromProto(storage));
}

// Tests that web::UserAgentType round trip correctly when serialized to proto.
TEST_F(SessionProtoUtilTest, UserAgentTypeRoundTrip) {
  static const UserAgentType kUserAgentTypes[] = {
      UserAgentType::NONE,
      UserAgentType::AUTOMATIC,
      UserAgentType::MOBILE,
      UserAgentType::DESKTOP,
  };

  for (UserAgentType user_agent_type : kUserAgentTypes) {
    EXPECT_EQ(user_agent_type,
              UserAgentTypeFromProto(UserAgentTypeToProto(user_agent_type)));
  }
}

// Tests that web::ReferrerPolicy round trip correctly when serialized to proto.
TEST_F(SessionProtoUtilTest, ReferrerPolicyRoundTrip) {
  static const ReferrerPolicy kReferrerPolicies[] = {
      ReferrerPolicyAlways,
      ReferrerPolicyDefault,
      ReferrerPolicyNoReferrerWhenDowngrade,
      ReferrerPolicyNever,
      ReferrerPolicyOrigin,
      ReferrerPolicyOriginWhenCrossOrigin,
      ReferrerPolicySameOrigin,
      ReferrerPolicyStrictOrigin,
      ReferrerPolicyStrictOriginWhenCrossOrigin,
  };

  for (ReferrerPolicy policy : kReferrerPolicies) {
    EXPECT_EQ(policy, ReferrerPolicyFromProto(ReferrerPolicyToProto(policy)));
  }
}

// Tests that web::Referrer round trip correctly when serialized to proto.
TEST_F(SessionProtoUtilTest, ReferrerRoundTrip) {
  const Referrer referrer(GURL("https://example.com/referrer"),
                          ReferrerPolicyDefault);

  proto::ReferrerStorage storage;
  SerializeReferrerToProto(referrer, storage);

  EXPECT_EQ(referrer, ReferrerFromProto(storage));
}

}  // anonymous namespace
}  // namespace web
