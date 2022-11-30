// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_REGISTRAR_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_REGISTRAR_H_

#include <vector>

#include "base/sequence_checker.h"
#include "content/common/content_export.h"

namespace content {

class NotificationObserver;
class NotificationSource;

// Aids in registering for notifications and ensures that all registered
// notifications are unregistered when the class is destroyed.
//
// The intended use is that you make a NotificationRegistrar member in your
// class and use it to register your notifications instead of going through the
// notification service directly. It will automatically unregister them for
// you.
class CONTENT_EXPORT NotificationRegistrar final {
 public:
  NotificationRegistrar();

  NotificationRegistrar(const NotificationRegistrar&) = delete;
  NotificationRegistrar& operator=(const NotificationRegistrar&) = delete;

  ~NotificationRegistrar();

  // Wrappers around NotificationService::[Add|Remove]Observer.
  void Add(NotificationObserver* observer,
           int type,
           const NotificationSource& source);
  void Remove(NotificationObserver* observer,
              int type,
              const NotificationSource& source);

  // Unregisters all notifications.
  void RemoveAll();

  // Returns true if no notifications are registered.
  bool IsEmpty() const;

  // Returns true if there is already a registered notification with the
  // specified details.
  bool IsRegistered(NotificationObserver* observer,
                    int type,
                    const NotificationSource& source);

 private:
  struct Record;

  // We keep registered notifications in a simple vector. This means we'll do
  // brute-force searches when removing them individually, but individual
  // removal is uncommon, and there will typically only be a couple of
  // notifications anyway.
  typedef std::vector<Record> RecordVector;

  // Lists all notifications we're currently registered for.
  RecordVector registered_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_REGISTRAR_H_
