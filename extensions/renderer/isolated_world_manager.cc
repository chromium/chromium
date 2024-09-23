// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/isolated_world_manager.h"

#include <map>
#include <string>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "extensions/common/mojom/execution_world.mojom.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/injection_host.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace extensions {

IsolatedWorldManager::IsolatedWorldInfo::IsolatedWorldInfo() = default;
IsolatedWorldManager::IsolatedWorldInfo::~IsolatedWorldInfo() = default;
IsolatedWorldManager::IsolatedWorldInfo::IsolatedWorldInfo(
    IsolatedWorldInfo&&) = default;

IsolatedWorldManager::PendingWorldInfo::PendingWorldInfo() = default;
IsolatedWorldManager::PendingWorldInfo::~PendingWorldInfo() = default;
IsolatedWorldManager::PendingWorldInfo::PendingWorldInfo(PendingWorldInfo&&) =
    default;

IsolatedWorldManager::IsolatedWorldManager() = default;
IsolatedWorldManager::~IsolatedWorldManager() = default;

IsolatedWorldManager& IsolatedWorldManager::GetInstance() {
  static base::NoDestructor<IsolatedWorldManager> g_instance;
  return *g_instance;
}

std::string IsolatedWorldManager::GetHostIdForIsolatedWorld(int world_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = isolated_worlds_.find(world_id);
  return iter != isolated_worlds_.end() ? iter->second.host_id : std::string();
}

std::optional<mojom::ExecutionWorld>
IsolatedWorldManager::GetExecutionWorldForIsolatedWorld(int world_id) {
  auto iter = isolated_worlds_.find(world_id);
  return iter != isolated_worlds_.end() ? std::optional<mojom::ExecutionWorld>(
                                              iter->second.execution_world)
                                        : std::nullopt;
}

void IsolatedWorldManager::RemoveIsolatedWorlds(const std::string& host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::erase_if(isolated_worlds_, [&host_id](const auto& entry) {
    return entry.second.host_id == host_id;
  });
}

void IsolatedWorldManager::SetUserScriptWorldProperties(
    const std::string& host_id,
    const std::optional<std::string>& world_id,
    std::optional<std::string> csp,
    bool enable_messaging) {
  PendingWorldInfo& pending_info =
      pending_worlds_info_[std::make_pair(host_id, world_id)];
  pending_info.csp = std::move(csp);
  pending_info.enable_messaging = enable_messaging;

  // Check if there are currently isolated worlds associated with the host. If
  // there are, we need to manually update them. This *won't* update already-
  // created worlds (CSPs are cached on the LocalDomWindow), but is necessary
  // to allow newly-created worlds to get the proper CSP before any injection
  // happens. Otherwise, new windows may eagerly create CSP for isolated worlds
  // (before the scripts have a chance to inject), resulting in stale values
  // even though the CSP should be updated.
  if (!pending_info.csp) {
    return;
  }

  for (auto& [blink_world_id, isolated_world] : isolated_worlds_) {
    if (isolated_world.host_id == host_id &&
        isolated_world.execution_world == mojom::ExecutionWorld::kUserScript &&
        isolated_world.world_id == world_id) {
      isolated_world.enable_messaging = enable_messaging;
      isolated_world.csp = pending_info.csp;

      blink::WebIsolatedWorldInfo info;
      info.security_origin =
          blink::WebSecurityOrigin::Create(isolated_world.url);
      info.human_readable_name =
          blink::WebString::FromUTF8(isolated_world.name);
      info.stable_id = blink::WebString::FromUTF8(host_id);
      info.content_security_policy =
          blink::WebString::FromUTF8(*pending_info.csp);
      blink::SetIsolatedWorldInfo(blink_world_id, info);
    }
  }
}

void IsolatedWorldManager::ClearUserScriptWorldProperties(
    const std::string& host_id,
    const std::optional<std::string>& world_id) {
  // Clear the pending world registration.
  pending_worlds_info_.erase(std::make_pair(host_id, world_id));

  // Determine if there's already an IsolatedWorldInfo for this world. If there
  // is, reset it to the default values from the user script configuration. This
  // ensures future worlds created from this config will have the proper state.
  IsolatedWorldInfo* world_info = FindIsolatedWorldInfo(
      host_id, mojom::ExecutionWorld::kUserScript, world_id);
  if (world_info) {
    world_info->csp = std::nullopt;
    world_info->enable_messaging = false;
  }
}

bool IsolatedWorldManager::IsMessagingEnabledInUserScriptWorld(
    int blink_world_id) {
  auto iter = isolated_worlds_.find(blink_world_id);
  return iter != isolated_worlds_.end() && iter->second.enable_messaging;
}

int IsolatedWorldManager::GetOrCreateIsolatedWorldForHost(
    const InjectionHost& injection_host,
    mojom::ExecutionWorld execution_world,
    const std::optional<std::string>& world_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static_assert(
      static_cast<int>(mojom::ExecutionWorld::kMaxValue) == 2,
      "You've added a new execution world! Does this code need to be updated?");
  CHECK_NE(mojom::ExecutionWorld::kMain, execution_world);

  static int g_next_blink_world_id =
      ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId();

  const std::string& host_id = injection_host.id().id;

  IsolatedWorldInfo* world_info =
      FindIsolatedWorldInfo(host_id, execution_world, world_id);

  if (!world_info) {
    int blink_world_id = g_next_blink_world_id++;
    // This map will tend to pile up over time, but realistically, you're never
    // going to have enough injection hosts for it to matter.
    // TODO(crbug.com/40262660): Are we sure about that? Processes can stick
    // around awhile.... (and this could be affected by introducing user script
    // worlds).
    world_info = &isolated_worlds_[blink_world_id];
    world_info->host_id = host_id;
    world_info->execution_world = execution_world;
    world_info->url = injection_host.url();
    world_info->name = injection_host.name();
    world_info->world_id = world_id;
    world_info->blink_world_id = blink_world_id;
  }

  // Initialize CSP and messaging for the new world. First, check if we have a
  // dedicated user script world configuration.
  const std::string* csp = nullptr;
  // The default for messaging depends on whether this is a user script world or
  // a content script world.
  world_info->enable_messaging =
      execution_world == mojom::ExecutionWorld::kIsolated;
  if (execution_world == mojom::ExecutionWorld::kUserScript) {
    PendingWorldInfo* pending_world_info =
        FindPendingWorldInfo(host_id, world_id);
    if (pending_world_info) {
      world_info->enable_messaging = pending_world_info->enable_messaging;
      if (pending_world_info->csp) {
        csp = &(*pending_world_info->csp);
      }
    }
  }

  // If we don't have a dedicated user script world, check the injection host's
  // default CSP.
  if (!csp) {
    csp = injection_host.GetContentSecurityPolicy();
  }

  // If we found a CSP, apply it to the world. Otherwise, explicitly clear out
  // any old CSP.
  world_info->csp = csp ? std::optional<std::string>(*csp) : std::nullopt;

  // Even though there may be an existing world for this `injection_host`'s id,
  // the properties may have changed (e.g. due to an extension update).
  // Overwrite any existing entries.
  UpdateBlinkIsolatedWorldInfo(world_info->blink_world_id, *world_info);

  return world_info->blink_world_id;
}

// static
bool IsolatedWorldManager::IsExtensionIsolatedWorld(int blink_world_id) {
  // Extension worlds are any that are higher than the lowest isolated world
  // value (values below that are reserved for other purposes).
  return blink_world_id >=
         ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId();
}

IsolatedWorldManager::IsolatedWorldInfo*
IsolatedWorldManager::FindIsolatedWorldInfo(
    const std::string& host_id,
    mojom::ExecutionWorld execution_world,
    const std::optional<std::string>& world_id) {
  auto iter = base::ranges::find_if(
      isolated_worlds_,
      [host_id, execution_world, world_id](const auto& entry) {
        return entry.second.host_id == host_id &&
               entry.second.execution_world == execution_world &&
               entry.second.world_id == world_id;
      });
  return iter != isolated_worlds_.end() ? &iter->second : nullptr;
}

IsolatedWorldManager::PendingWorldInfo*
IsolatedWorldManager::FindPendingWorldInfo(
    const std::string& host_id,
    const std::optional<std::string>& world_id) {
  auto iter = pending_worlds_info_.find(std::make_pair(host_id, world_id));
  return iter != pending_worlds_info_.end() ? &(iter->second) : nullptr;
}

void IsolatedWorldManager::UpdateBlinkIsolatedWorldInfo(
    int world_id,
    const IsolatedWorldInfo& world_info) {
  blink::WebIsolatedWorldInfo blink_info;
  blink_info.security_origin = blink::WebSecurityOrigin::Create(world_info.url);
  blink_info.human_readable_name = blink::WebString::FromUTF8(world_info.name);
  blink_info.stable_id = blink::WebString::FromUTF8(world_info.host_id);
  if (world_info.csp) {
    blink_info.content_security_policy =
        blink::WebString::FromUTF8(*world_info.csp);
  }
  blink::SetIsolatedWorldInfo(world_id, blink_info);
}

}  // namespace extensions
