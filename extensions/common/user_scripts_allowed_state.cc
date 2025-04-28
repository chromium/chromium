// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/user_scripts_allowed_state.h"

#include <map>

#include "base/containers/map_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// A map of context ids to sets of extension ids. An extension id being present
// in the set indicates the user scripts API is allowed for the extension (in
// that context only).
using CurrentUserScriptAllowedMap = std::map<int, std::map<ExtensionId, bool>>;

namespace {

// The user script allowed state map can be checked by different threads
// (e.g. the renderer process main thread and worker thread). Ensure
// thread-safety by locking.
base::Lock& GetUserScriptAllowedMapLock() {
  static base::NoDestructor<base::Lock> g_lock;
  return *g_lock;
}

CurrentUserScriptAllowedMap& GetUserScriptAllowedMap() {
  GetUserScriptAllowedMapLock().AssertAcquired();
  static base::NoDestructor<CurrentUserScriptAllowedMap> map;
  return *map;
}

}  // namespace

std::optional<bool> GetCurrentUserScriptAllowedState(
    int context_id,
    const ExtensionId& extension_id) {
  base::AutoLock lock(GetUserScriptAllowedMapLock());
  CurrentUserScriptAllowedMap& map = GetUserScriptAllowedMap();
  auto context_id_iter = map.find(context_id);

  if (context_id_iter == map.end()) {
    return std::nullopt;
  }

  auto extension_id_map = context_id_iter->second;
  auto extension_id_iter = extension_id_map.find(extension_id);

  if (extension_id_iter == extension_id_map.end()) {
    return std::nullopt;
  }

  return extension_id_iter->second;
}

void SetCurrentUserScriptAllowedState(int context_id,
                                      const ExtensionId& extension_id,
                                      bool user_script_allowed_state) {
  base::AutoLock lock(GetUserScriptAllowedMapLock());
  CurrentUserScriptAllowedMap& map = GetUserScriptAllowedMap();
  map[context_id][extension_id] = user_script_allowed_state;
}

}  // namespace extensions
