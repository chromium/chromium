// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SEQUENCE_MANAGER_CONFIGURATOR_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SEQUENCE_MANAGER_CONFIGURATOR_H_

#include "base/component_export.h"
#include "base/threading/thread.h"

namespace network {

// Configures the sequence manager settings in Thread `options` for use with the
// network service task scheduler. This must be called before the thread is
// started.
COMPONENT_EXPORT(NETWORK_CPP)
void ConfigureSequenceManager(base::Thread::Options& options);

// Returns true if `ConfigureSequenceManager()` has been called
// for the current thread.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsSequenceManagerConfigured();

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SEQUENCE_MANAGER_CONFIGURATOR_H_
