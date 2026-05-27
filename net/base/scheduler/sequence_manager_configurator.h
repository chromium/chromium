// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEDULER_SEQUENCE_MANAGER_CONFIGURATOR_H_
#define NET_BASE_SCHEDULER_SEQUENCE_MANAGER_CONFIGURATOR_H_

#include "base/threading/thread.h"
#include "net/base/net_export.h"

namespace net {

// Configures the sequence manager settings in Thread `options` for use with the
// net task scheduler. This must be called before the thread is
// started.
NET_EXPORT void ConfigureSequenceManager(base::Thread::Options& options);

// Returns true if `ConfigureSequenceManager()` has been called
// for the current thread.
NET_EXPORT bool IsSequenceManagerConfigured();

}  // namespace net

#endif  // NET_BASE_SCHEDULER_SEQUENCE_MANAGER_CONFIGURATOR_H_
