// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/extension_function_crash_keys.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/crash/core/common/crash_key.h"
#include "extensions/common/extension_id.h"

namespace extensions::extension_function_crash_keys {
namespace {

struct CallInfo {
  int count = 0;              // Number of in-flight calls.
  base::TimeTicks timestamp;  // Time of the last call.
};

// Returns a map from an extension ID to information about in-flight calls to
// ExtensionFunction. Uses base::flat_map<> because the map is typically small
// (0 or 1 item) and the size is bounded by the number of installed extensions.
// NOTE: This approach isn't perfect. In particular, this call sequence ends up
// with slightly odd reporting:
// - API A start (1)
// - API B start
// - API A start (2)
// - API A end (2)
// This will report crash keys in the order (API A, API B) even though the most
// recent API A call has completed. This seemms OK because it's true that API A
// was the most recently called. It also avoids storing a stack of all in-flight
// API calls with per-call IDs to match them up. During startup when extensions
// are initializing there can be hundreds of in-flight calls.
base::flat_map<ExtensionId, CallInfo>& ExtensionIdToCallInfoMap() {
  static base::NoDestructor<base::flat_map<ExtensionId, CallInfo>> instance;
  return *instance;
}

// Updates the crash keys for extensions with in-flight ExtensionFunction calls.
void UpdateCrashKeys() {
  // Extract the call timestamps and extension IDs into a vector for sorting.
  // Use ExtensionId* to avoid copying the string IDs.
  const auto& map = ExtensionIdToCallInfoMap();
  std::vector<std::pair<base::TimeTicks, const ExtensionId*>> calls;
  calls.reserve(map.size());
  for (const auto& entry : map) {
    calls.emplace_back(entry.second.timestamp, &entry.first);
  }
  // Sort most recent calls to the front of the vector.
  std::sort(calls.begin(), calls.end(), std::greater<>());
  // Set up crash keys.
  using ArrayItemKey = crash_reporter::CrashKeyString<64>;
  static ArrayItemKey crash_keys[] = {
      {"extension-function-caller-1", ArrayItemKey::Tag::kArray},
      {"extension-function-caller-2", ArrayItemKey::Tag::kArray},
      {"extension-function-caller-3", ArrayItemKey::Tag::kArray},
  };
  // Store up to 3 crash keys with extension IDs.
  int index = 0;
  for (auto it = calls.begin(); it != calls.end() && index < 3; ++it, ++index) {
    const ExtensionId* extension_id = it->second;
    crash_keys[index].Set(*extension_id);
  }
  // Clear the remaining crash keys.
  for (; index < 3; ++index) {
    crash_keys[index].Clear();
  }
}

}  // namespace

void StartExtensionFunctionCall(const ExtensionId& extension_id) {
  base::TimeTicks now = base::TimeTicks::Now();
  auto& map = ExtensionIdToCallInfoMap();
  auto it = map.find(extension_id);
  if (it == map.end()) {
    map[extension_id] = {.count = 1, .timestamp = now};
  } else {
    it->second.count++;
    it->second.timestamp = now;
  }
  UpdateCrashKeys();
}

void EndExtensionFunctionCall(const ExtensionId& extension_id) {
  auto& map = ExtensionIdToCallInfoMap();
  auto it = map.find(extension_id);
  CHECK(it != map.end());
  int new_count = --it->second.count;
  CHECK_GE(new_count, 0);
  if (new_count == 0) {
    map.erase(it);
    UpdateCrashKeys();
  }
}

}  // namespace extensions::extension_function_crash_keys
