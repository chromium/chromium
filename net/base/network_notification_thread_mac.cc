// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_notification_thread_mac.h"

#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"

namespace net {

namespace {

class NotificationThreadMac {
 public:
  NotificationThreadMac(const NotificationThreadMac&) = delete;
  NotificationThreadMac& operator=(const NotificationThreadMac&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

 private:
  friend base::NoDestructor<NotificationThreadMac>;

  NotificationThreadMac() : thread_("NetworkNotificationThreadMac") {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::UI;
    options.joinable = false;
    thread_.StartWithOptions(std::move(options));
    task_runner_ = thread_.task_runner();
    thread_.DetachFromSequence();
  }

  ~NotificationThreadMac() = delete;

  // The |thread_| object is not thread-safe. This should not be accessed
  // outside the constructor.
  base::Thread thread_;

  // Saved TaskRunner handle that can be accessed from any thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace

scoped_refptr<base::SingleThreadTaskRunner> GetNetworkNotificationThreadMac() {
  static base::NoDestructor<NotificationThreadMac> notification_thread;
  return notification_thread->task_runner();
}

}  // namespace net
