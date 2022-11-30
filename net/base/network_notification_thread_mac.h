// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_NOTIFICATION_THREAD_MAC_H_
#define NET_BASE_NETWORK_NOTIFICATION_THREAD_MAC_H_

#include "base/task/single_thread_task_runner.h"

namespace net {

// Returns a TaskRunner that runs on a TYPE_UI thread, for macOS notification
// APIs that require a CFRunLoop. The thread is not joined on shutdown (like
// TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN), so any users of this thread
// must take care not to access invalid objects during shutdown.
scoped_refptr<base::SingleThreadTaskRunner> GetNetworkNotificationThreadMac();

}  // namespace net

#endif  // NET_BASE_NETWORK_NOTIFICATION_THREAD_MAC_H_
