// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_FAKE_SYNCED_SESSIONS_H_
#define IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_FAKE_SYNCED_SESSIONS_H_

#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"

namespace synced_sessions {

// A fake SyncedSessions for testing purpose.
class FakeSyncedSessions : public SyncedSessions {
 public:
  FakeSyncedSessions();

  ~FakeSyncedSessions();

  // Adds a distant session.
  void AddSession(std::unique_ptr<const DistantSession> session);
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_FAKE_SYNCED_SESSIONS_H_
