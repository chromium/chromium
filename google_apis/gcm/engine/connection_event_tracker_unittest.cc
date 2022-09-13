// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_event_tracker.h"

#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

class ConnectionEventTrackerTest : public testing::Test {
 public:
  ConnectionEventTracker* tracker() { return &tracker_; }

 private:
  ConnectionEventTracker tracker_;
};

}  // namespace

TEST_F(ConnectionEventTrackerTest, SuccessfulAttempt) {
  tracker()->StartConnectionAttempt();
  tracker()->ConnectionAttemptSucceeded();
  tracker()->EndConnectionAttempt();

  mcs_proto::LoginRequest request;
  tracker()->WriteToLoginRequest(&request);

  ASSERT_EQ(request.client_event().size(), 1);
  for (const auto& event : request.client_event())
    EXPECT_EQ(event.type(), mcs_proto::ClientEvent::SUCCESSFUL_CONNECTION);
}

}  // namespace gcm
