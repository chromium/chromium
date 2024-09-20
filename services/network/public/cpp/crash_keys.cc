// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/crash_keys.h"

#include <utility>

#include "base/debug/crash_logging.h"

namespace network::debug {

namespace {
base::debug::CrashKeyString* GetCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "network_deserialization", base::debug::CrashKeySize::Size32);
  return crash_key;
}
}  // namespace

void SetDeserializationCrashKeyString(std::string_view str) {
  base::debug::SetCrashKeyString(GetCrashKey(), str);
}

void ClearDeserializationCrashKeyString() {
  base::debug::ClearCrashKeyString(GetCrashKey());
}

}  // namespace network::debug
