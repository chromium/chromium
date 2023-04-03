// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_ISOLATED_WORLD_MANAGER_H_
#define EXTENSIONS_RENDERER_ISOLATED_WORLD_MANAGER_H_

#include <map>
#include <string>

#include "base/sequence_checker.h"

class InjectionHost;

namespace extensions {

// A class responsible for managing the creation and maintenance of isolated
// worlds related to extensions APIs.
//
// There is a single instance of this class (per renderer), retrieved via
// `IsolatedWorldManager::GetInstance()`.
//
// This class is *not* thread-safe and is only designed to be accessed from the
// main renderer thread; this is not an issue because all
// script-injection-related functionality happens on the main thread (e.g.,
// there is no situation in which an extension service worker directly accesses
// isolated worlds).
//
// A note on Host IDs: Host IDs are unique identifiers for the entity causing
// the injection. This is *usually* an extension, but can also be WebUI in the
// case of WebUI injecting into an embedded <webview>. The Host ID for an
// extension is the extension's ID.
class IsolatedWorldManager {
 public:
  IsolatedWorldManager();
  IsolatedWorldManager(const IsolatedWorldManager&) = delete;
  IsolatedWorldManager& operator=(const IsolatedWorldManager&) = delete;
  ~IsolatedWorldManager();

  // Returns the shared instance of the isolated world manager.
  static IsolatedWorldManager& GetInstance();

  // Returns the id of the injection host associated with the given `world_id`
  // or an empty string if none exists.
  std::string GetHostIdForIsolatedWorld(int world_id);

  // Removes the isolated world associated with the given `host_id`, if any
  // exists.
  void RemoveIsolatedWorld(const std::string& host_id);

  // Returns the id of the isolated world associated with the given
  // `injection_host`.  If none exists, creates a new world for it associated
  // with the host's name and CSP.
  int GetOrCreateIsolatedWorldForHost(const InjectionHost& injection_host);

 private:
  // A map between injection host ID and isolated world ID.
  using IsolatedWorldMap = std::map<std::string, int>;

  IsolatedWorldMap isolated_worlds_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_ISOLATED_WORLD_MANAGER_H_
