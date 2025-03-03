// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_FEATURES_H_
#define CONTENT_COMMON_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_ANDROID)

namespace content::features {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserLevelMemoryPressureSignalOn3GbDevices);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserLevelMemoryPressureSignalOn4GbDevices);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kUserLevelMemoryPressureSignalOn6GbDevices);

// Helper functions for UserLevelMemoryPressureSignal features.
CONTENT_EXPORT bool IsUserLevelMemoryPressureSignalEnabledOn3GbDevices();
CONTENT_EXPORT bool IsUserLevelMemoryPressureSignalEnabledOn4GbDevices();
CONTENT_EXPORT bool IsUserLevelMemoryPressureSignalEnabledOn6GbDevices();

CONTENT_EXPORT base::TimeDelta InertIntervalFor3GbDevices();
CONTENT_EXPORT base::TimeDelta InertIntervalFor4GbDevices();
CONTENT_EXPORT base::TimeDelta InertIntervalFor6GbDevices();

CONTENT_EXPORT base::TimeDelta MinUserMemoryPressureIntervalOn3GbDevices();
CONTENT_EXPORT base::TimeDelta MinUserMemoryPressureIntervalOn4GbDevices();
CONTENT_EXPORT base::TimeDelta MinUserMemoryPressureIntervalOn6GbDevices();

}  // namespace content::features

#endif  // BUILDFLAG(IS_ANDROID)

#endif  // CONTENT_COMMON_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_FEATURES_H_
