// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BACKEND_EXPERIMENT_H_
#define NET_DISK_CACHE_BACKEND_EXPERIMENT_H_

#include "build/build_config.h"
#include "net/base/net_export.h"

namespace disk_cache {

// True if the current platform already uses Simple disk cache backend by
// default.
constexpr bool IsSimpleBackendEnabledByDefaultPlatform() {
  return BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
         BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC);
}

// True if assigned to any of disk cache backend experiment groups.
NET_EXPORT bool InBackendExperiment();

// True if assigned to the "simple" disk cache backend group.
NET_EXPORT bool InSimpleBackendExperimentGroup();

// True if assigned to the "blockfile" disk cache backend group.
NET_EXPORT bool InBlockfileBackendExperimentGroup();

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BACKEND_EXPERIMENT_H_
