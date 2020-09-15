// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_EMBEDDER_SWITCHES_H_
#define SERVICES_SERVICE_MANAGER_EMBEDDER_SWITCHES_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace service_manager {
namespace switches {

#if defined(OS_WIN)
COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER_SWITCHES)
extern const char kDefaultServicePrefetchArgument[];
#endif  // defined(OS_WIN)

COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER_SWITCHES)
extern const char kDisableInProcessStackTraces[];

COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER_SWITCHES)
extern const char kEnableLogging[];

COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER_SWITCHES)
extern const char kProcessType[];

COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER_SWITCHES)
extern const char kServiceRequestChannelToken[];

COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER_SWITCHES)
extern const char kSharedFiles[];

COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER_SWITCHES)
extern const char kZygoteProcess[];

}  // namespace switches
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_EMBEDDER_SWITCHES_H_
