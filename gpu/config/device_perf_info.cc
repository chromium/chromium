// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/device_perf_info.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace gpu {

namespace {
// Global instance in browser process.
std::optional<DevicePerfInfo> g_device_perf_info;

base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}
}  // namespace

std::optional<DevicePerfInfo> GetDevicePerfInfo() {
  base::AutoLock lock(GetLock());
  return g_device_perf_info;
}

void SetDevicePerfInfo(const DevicePerfInfo& device_perf_info) {
  base::AutoLock lock(GetLock());
  g_device_perf_info = device_perf_info;
}

}  // namespace gpu
