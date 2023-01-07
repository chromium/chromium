// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_notification_tracker.h"

#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace content {

TestNotificationTracker::Event::Event()
    : type(NOTIFICATION_ALL),
      source(NotificationService::AllSources()),
      details(NotificationService::NoDetails()) {
}
TestNotificationTracker::Event::Event(int t,
                                      NotificationSource s,
                                      NotificationDetails d)
    : type(t),
      source(s),
      details(d) {
}

TestNotificationTracker::TestNotificationTracker() {
}

TestNotificationTracker::~TestNotificationTracker() {
}

void TestNotificationTracker::ListenFor(
    int type,
    const NotificationSource& source) {
  registrar_.Add(this, type, source);
}

void TestNotificationTracker::Reset() {
  events_.clear();
}

bool TestNotificationTracker::Check1AndReset(int type) {
  if (size() != 1) {
    Reset();
    return false;
  }
  bool success = events_[0].type == type;
  Reset();
  return success;
}

bool TestNotificationTracker::Check2AndReset(int type1,
                                             int type2) {
  if (size() != 2) {
    Reset();
    return false;
  }
  bool success = events_[0].type == type1 && events_[1].type == type2;
  Reset();
  return success;
}

bool TestNotificationTracker::Check3AndReset(int type1,
                                             int type2,
                                             int type3) {
  if (size() != 3) {
    Reset();
    return false;
  }
  bool success = events_[0].type == type1 &&
                 events_[1].type == type2 &&
                 events_[2].type == type3;
  Reset();
  return success;
}

void TestNotificationTracker::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  events_.push_back(Event(type, source, details));
}

}  // namespace content
