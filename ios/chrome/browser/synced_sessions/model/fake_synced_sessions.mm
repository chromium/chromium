// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/model/fake_synced_sessions.h"

#import "ios/chrome/browser/synced_sessions/model/distant_session.h"

namespace synced_sessions {

FakeSyncedSessions::FakeSyncedSessions() = default;

FakeSyncedSessions::~FakeSyncedSessions() = default;

void FakeSyncedSessions::AddSession(
    std::unique_ptr<const DistantSession> session) {
  sessions_.push_back(std::move(session));
}

}  // namespace synced_sessions
