// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_CRASH_KEYS_H_
#define SERVICES_VIZ_PUBLIC_CPP_CRASH_KEYS_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "components/crash/core/common/crash_buildflags.h"

namespace viz {

// Sets a crash key to indicate what structure triggered a deserialization error
// in viz mojom code. Up to 3 crash keys are stored in order of invocation
// ("viz_deserialization", "viz_deserialization_2", "viz_deserialization_3").
//
// These crash keys should be cleared after a bad message is handled by calling
// ClearDeserializationCrashKeys().
COMPONENT_EXPORT(VIZ_CRASH_KEYS)
void SetDeserializationCrashKeyString(std::string_view str);

// Clears the deserialization crash keys.
COMPONENT_EXPORT(VIZ_CRASH_KEYS) void ClearDeserializationCrashKeys();

#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
// Helper for unit tests to inspect crash key values. Because Crashpad's
// AnnotationList is maintained per-module in Windows component builds, calling
// crash_reporter::GetCrashKeyValue() directly from a test executable cannot
// inspect crash keys set inside the viz_crash_keys DLL component. This helper
// ensures the lookup executes within the viz_crash_keys component module.
COMPONENT_EXPORT(VIZ_CRASH_KEYS)
std::string GetDeserializationCrashKeyValueForTesting(
    std::string_view key_name);
#endif  // BUILDFLAG(USE_CRASHPAD_ANNOTATION)

}  // namespace viz

#endif  // SERVICES_VIZ_PUBLIC_CPP_CRASH_KEYS_H_
