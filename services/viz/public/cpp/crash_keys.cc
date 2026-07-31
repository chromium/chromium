// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/crash_keys.h"

#include <string>
#include <string_view>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "components/crash/core/common/crash_buildflags.h"
#include "components/crash/core/common/crash_key.h"

namespace viz {

namespace {

base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// Count of crash keys set within the current deserialization context.
size_t g_keys_set_count = 0;

crash_reporter::CrashKeyString<128>& GetVizDeserializationCrashKey(
    size_t index) {
  // Process-global crash keys registered with the crash reporter.
  // "viz_deserialization" is used for the 1st error for backward compatibility.
  static crash_reporter::CrashKeyString<128> key1("viz_deserialization");
  static crash_reporter::CrashKeyString<128> key2("viz_deserialization_2");
  static crash_reporter::CrashKeyString<128> key3("viz_deserialization_3");
  switch (index) {
    case 0:
      return key1;
    case 1:
      return key2;
    case 2:
      return key3;
    default:
      NOTREACHED();
  }
}

}  // namespace

void SetDeserializationCrashKeyString(std::string_view str) {
  base::AutoLock lock(GetLock());
  // Store up to 3 crash keys in order of invocation.
  if (g_keys_set_count < 3) {
    GetVizDeserializationCrashKey(g_keys_set_count).Set(str);
    g_keys_set_count++;
  }
}

void ClearDeserializationCrashKeys() {
  base::AutoLock lock(GetLock());
  GetVizDeserializationCrashKey(0).Clear();
  GetVizDeserializationCrashKey(1).Clear();
  GetVizDeserializationCrashKey(2).Clear();
  g_keys_set_count = 0;
}

#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
std::string GetDeserializationCrashKeyValueForTesting(
    std::string_view key_name) {
  base::AutoLock lock(GetLock());
  // Directly iterate over the static crash key instances rather than calling
  // crash_reporter::GetCrashKeyValue(), because GetCrashKeyValue() is only
  // declared when UNIT_TEST is defined (which is not the case when compiling
  // this production component).
  for (size_t i = 0; i < 3; ++i) {
    auto& key = GetVizDeserializationCrashKey(i);
    if (key_name == key.name()) {
      if (!key.is_set()) {
        return std::string();
      }
      return std::string(key.value());
    }
  }
  return std::string();
}
#endif  // BUILDFLAG(USE_CRASHPAD_ANNOTATION)

}  // namespace viz
