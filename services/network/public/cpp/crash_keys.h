// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CRASH_KEYS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CRASH_KEYS_H_

#include <string_view>

#include "base/component_export.h"

namespace network {
namespace debug {

COMPONENT_EXPORT(NETWORK_CPP_CRASH_KEYS)
void SetDeserializationCrashKeyString(std::string_view str);

COMPONENT_EXPORT(NETWORK_CPP_CRASH_KEYS)
void ClearDeserializationCrashKeyString();

}  // namespace debug
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CRASH_KEYS_H_
