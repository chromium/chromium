// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_system_android.h"

#include "crazy_linker_debug.h"

#ifndef __ANDROID__
#error "This source file should only be compiled for Android."
#endif

#include <stdlib.h>
#include <sys/system_properties.h>

namespace crazy {

// Technical note regarding reading system properties.
//
// Try to use the new __system_property_read_callback API that appeared in
// Android O / API level 26 when available. Otherwise use the deprecated
// __system_property_get function.
//
// For more technical details from an NDK maintainer, see:
// https://bugs.chromium.org/p/chromium/issues/detail?id=392191#c17

// Use a weak symbol import to resolve at runtime whether the function is
// available.
extern "C" void __system_property_read_callback(
    const prop_info* info,
    void (*callback)(void* cookie,
                     const char* name,
                     const char* value,
                     uint32_t serial),
    void* cookie) __attribute__((weak));

static int GetSystemPropertyAsInt(const char* name) {
  int result = 0;
  if (__system_property_read_callback) {
    const prop_info* info = __system_property_find(name);
    if (info) {
      auto callback = [](void* cookie, const char*, const char* value,
                         uint32_t) { *(int*)cookie = atoi(value); };
      __system_property_read_callback(info, callback, &result);
    }
  } else {
    char value[PROP_VALUE_MAX] = {};
    if (__system_property_get(name, value) >= 1)
      result = atoi(value);
  }
  return result;
}

int GetAndroidDeviceApiLevel() {
  static int s_api_level = -1;
  if (s_api_level < 0) {
    s_api_level = GetSystemPropertyAsInt("ro.build.version.sdk");
    LOG("Device API level: %d", s_api_level);
  }
  return s_api_level;
}

}  // namespace crazy
