// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_ISOLATED_WORLD_MANAGER_H_
#define EXTENSIONS_RENDERER_ISOLATED_WORLD_MANAGER_H_

#include <map>
#include <optional>
#include <string>
#include "base/sequence_checker.h"
#include "extensions/common/mojom/execution_world.mojom.h"
#include "url/gurl.h"

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

  // Returns the execution world for the given `world_id`, if any exists.
  std::optional<mojom::ExecutionWorld> GetExecutionWorldForIsolatedWorld(
      int world_id);

  // Removes all isolated worlds associated with the given `host_id`, if any
  // exist.
  void RemoveIsolatedWorlds(const std::string& host_id);

  // Sets properties for newly-created user script worlds for the associated
  // `host_id`. Does not affect any already-created worlds, but does update the
  // world data stored in blink so that any newly-created worlds will be
  // properly initialized.
  void SetUserScriptWorldProperties(const std::string& host_id,
                                    std::optional<std::string> csp,
                                    bool enable_messaging);

  // Returns whether messaging APIs should be enabled in worlds for the given
  // `host_id`.
  bool IsMessagingEnabledInUserScriptWorlds(const std::string& host_id);

  // Returns the id of the isolated world associated with the given
  // `injection_host`.  If none exists, creates a new world for it associated
  // with the host's name and CSP.
  int GetOrCreateIsolatedWorldForHost(const InjectionHost& injection_host,
                                      mojom::ExecutionWorld execution_world);

 private:
  // A structure to track existing isolated world information.
  struct IsolatedWorldInfo {
    // Defaults, to appease the Chromium clang plugin.
    IsolatedWorldInfo();
    ~IsolatedWorldInfo();
    IsolatedWorldInfo(IsolatedWorldInfo&&);

    // The id of the injection host the world is associated with. For
    // extensions, this is the extension ID.
    std::string host_id;

    // The execution world for the isolated world. Currently, this is restricted
    // to mojom::ExecutionWorld::kIsolated.
    mojom::ExecutionWorld execution_world;

    // A human-friendly name for the isolated world host.
    std::string name;

    // The URL associated with the isolated world host.
    GURL url;

    // CSP to use for the isolated world, if any.
    std::optional<std::string> csp;
  };

  // A set of data to store properties for newly-created isolated worlds.
  struct PendingWorldInfo {
    PendingWorldInfo();
    ~PendingWorldInfo();
    PendingWorldInfo(PendingWorldInfo&&);

    // The CSP to use for newly-created isolated worlds, if any.
    std::optional<std::string> csp;

    // Whether to enable messaging APIs in newly-created isolated worlds.
    bool enable_messaging = false;
  };

  void UpdateBlinkIsolatedWorldInfo(int world_id,
                                    const IsolatedWorldInfo& world_info);

  // A map between the isolated world ID and injection host ID.
  using IsolatedWorldMap = std::map<int, IsolatedWorldInfo>;
  IsolatedWorldMap isolated_worlds_;

  // A map of <host id, info> to use for newly-created isolated worlds.
  std::map<std::string, PendingWorldInfo> pending_worlds_info_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_ISOLATED_WORLD_MANAGER_H_
