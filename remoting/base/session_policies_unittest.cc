// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_policies.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(SessionPolicies, Equality) {
  SessionPolicies test_policies_1;
  test_policies_1.clipboard_size_bytes = 1024;
  test_policies_1.host_udp_port_range = *PortRange::Create(123, 456);
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
  test_policies_5.host_udp_port_range = *PortRange::Create(123, 789);
  EXPECT_NE(test_policies_1, test_policies_5);

  SessionPolicies test_policies_6 = test_policies_1;
  test_policies_6.allow_terminal_mode = false;
  EXPECT_NE(test_policies_1, test_policies_6);
}

}  // namespace remoting
