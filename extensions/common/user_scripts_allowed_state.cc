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
using CurrentUserScriptAllowedMap = std::map<int, std::set<ExtensionId>>;

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

bool GetCurrentUserScriptAllowedState(int context_id,
                                      const ExtensionId& extension_id) {
  base::AutoLock lock(GetUserScriptAllowedMapLock());
  CurrentUserScriptAllowedMap& map = GetUserScriptAllowedMap();
  auto iter = map.find(context_id);
  return iter != map.end() && iter->second.contains(extension_id);
}

void SetCurrentUserScriptAllowedState(int context_id,
                                      const ExtensionId& extension_id,
                                      bool user_script_allowed_state) {
  base::AutoLock lock(GetUserScriptAllowedMapLock());
  CurrentUserScriptAllowedMap& map = GetUserScriptAllowedMap();

  // Adding an extension.
  if (user_script_allowed_state) {
    map[context_id].insert(extension_id);
    return;
  }

  // Removing an extension.
  auto iter = map.find(context_id);

  // If key not found then the extension isn't allowed so there's nothing to do.
  if (iter == map.end()) {
    return;
  }

  // Delete the extension so it is no longer allowed.
  iter->second.erase(extension_id);

  // If no more extensions to track delete the (now unused) key.
  if (iter->second.empty()) {
    map.erase(iter);
  }
}

}  // namespace extensions
