// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file describes a central switchboard for notifications that might
// happen in various parts of the application, and allows users to register
// observers for various classes of events that they're interested in.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_SERVICE_H_

#include "content/common/content_export.h"
#include "content/public/browser/notification_source.h"

namespace content {

class CONTENT_EXPORT NotificationService {
 public:
  static std::unique_ptr<NotificationService> CreateIfNecessaryForTesting();

  virtual ~NotificationService() = default;

  // Returns a NotificationSource that represents all notification sources
  // (for the purpose of registering an observer for events from all sources).
  static Source<void> AllSources() { return Source<void>(nullptr); }

  // Returns the same value as AllSources(). This function has semantic
  // differences to the programmer: We have checked that this AllSources()
  // usage is safe in the face of multiple profiles. Objects that were
  // singletons now will always have multiple instances, one per browser
  // context.
  //
  // Some usage is safe, where the Source is checked to see if it's a member of
  // a container before use. But, we want the number of AllSources() calls to
  // drop to almost nothing, because most usages are not multiprofile safe and
  // were done because it was easier to listen to everything.
  static Source<void> AllBrowserContextsAndSources() {
    return Source<void>(nullptr);
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_SERVICE_H_
