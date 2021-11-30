// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NOTIFICATION_TRACKER_H_
#define CONTENT_PUBLIC_TEST_TEST_NOTIFICATION_TRACKER_H_

#include <stddef.h>

#include <vector>

#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"

namespace content {

// Provides an easy way for tests to verify that a given set of notifications
// was received during test execution.
class TestNotificationTracker : public NotificationObserver {
 public:
  // Records one received notification.
  struct Event {
    Event();
    Event(int t, NotificationSource s, NotificationDetails d);

    int type;
    NotificationSource source;
    NotificationDetails details;
  };

  // By default, it won't listen for any notifications. You'll need to call
  // ListenFor for the notifications you are interested in.
  TestNotificationTracker();

  TestNotificationTracker(const TestNotificationTracker&) = delete;
  TestNotificationTracker& operator=(const TestNotificationTracker&) = delete;

  ~TestNotificationTracker() override;

  // Makes this object listen for the given notification with the given source.
  //
  // To listen for all sources, pass
  // NotificationService::AllBrowserContextsAndSources() as the |source|.
  void ListenFor(int type, const NotificationSource& source);

  // Clears the list of events.
  void Reset();

  // Given notifications type(sp, returns true if the list of notifications
  // were exactly those listed in the given arg(s), and in the same order.
  //
  // This will also reset the list so that the next call will only check for
  // new notifications. Example:
  //   <do stuff>
  //   Check1AndReset(NOTIFY_A);
  //   <do stuff>
  //   Check2AndReset(NOTIFY_B, NOTIFY_C)
  bool Check1AndReset(int type);
  bool Check2AndReset(int type1,
                      int type2);
  bool Check3AndReset(int type1,
                      int type2,
                      int type3);

  // Returns the number of notifications received since the last reset.
  size_t size() const { return events_.size(); }

  // Returns the information about the event at the given index. The index must
  // be in [0, size).
  const Event& at(size_t i) const { return events_[i]; }

 protected:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

 private:
  NotificationRegistrar registrar_;

  // Lists all received since last cleared, in the order they were received.
  std::vector<Event> events_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NOTIFICATION_TRACKER_H_
