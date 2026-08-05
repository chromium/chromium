// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_policies.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(SessionPolicies, Equality) {
  SessionPolicies test_policies_1;
  test_policies_1.clipboard_size_bytes = 1024;
  test_policies_1.host_udp_port_range = {.min_port = 123, .max_port = 456};
  test_policies_1.allow_file_transfer = true;
  test_policies_1.maximum_session_duration = base::Hours(20);
  test_policies_1.curtain_required = false;
  test_policies_1.host_username_match_required = true;
  test_policies_1.allow_remote_input = false;

  SessionPolicies test_policies_2 = test_policies_1;
  EXPECT_EQ(test_policies_1, test_policies_2);

  SessionPolicies test_policies_3 = test_policies_1;
  test_policies_3.allow_file_transfer.reset();
  EXPECT_NE(test_policies_1, test_policies_3);

  SessionPolicies test_policies_4 = test_policies_1;
  test_policies_4.maximum_session_duration = base::Hours(10);
  EXPECT_NE(test_policies_1, test_policies_4);

  SessionPolicies test_policies_5 = test_policies_1;
  test_policies_5.host_udp_port_range.max_port = 789;
  EXPECT_NE(test_policies_1, test_policies_5);
}

TEST(SessionPolicies, Validate) {
  EXPECT_TRUE(SessionPolicies().Validate().has_value());

  SessionPolicies valid_policies;
  valid_policies.maximum_session_duration =
      SessionPolicies::kMinMaximumSessionDuration;
  valid_policies.host_udp_port_range = PortRange{1000, 2000};
  EXPECT_TRUE(valid_policies.Validate().has_value());

#if !BUILDFLAG(IS_CHROMEOS)
  SessionPolicies sub_boundary_duration;
  sub_boundary_duration.maximum_session_duration = base::Minutes(29);
  EXPECT_FALSE(sub_boundary_duration.Validate().has_value());
  EXPECT_THAT(sub_boundary_duration.Validate().error().ToString(),
              testing::HasSubstr("maximum_session_duration"));
#else
  SessionPolicies short_duration;
  short_duration.maximum_session_duration = base::Minutes(10);
  EXPECT_TRUE(short_duration.Validate().has_value());
#endif

  SessionPolicies invalid_port_range;
  invalid_port_range.host_udp_port_range = PortRange{100, 50};
  EXPECT_FALSE(invalid_port_range.Validate().has_value());
  EXPECT_THAT(invalid_port_range.Validate().error().ToString(),
              testing::HasSubstr("UDP port range"));

  SessionPolicies combined_invalid;
#if !BUILDFLAG(IS_CHROMEOS)
  combined_invalid.maximum_session_duration = base::Minutes(15);
#endif
  combined_invalid.host_udp_port_range = PortRange{1, 0};
  EXPECT_FALSE(combined_invalid.Validate().has_value());
}

}  // namespace remoting
