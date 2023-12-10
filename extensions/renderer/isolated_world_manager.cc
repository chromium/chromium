// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/isolated_world_manager.h"

#include <map>
#include <string>

#include "base/check.h"
#include "base/containers/cxx20_erase_map.h"
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

  base::EraseIf(isolated_worlds_, [&host_id](const auto& entry) {
    return entry.second.host_id == host_id;
  });
}

void IsolatedWorldManager::SetUserScriptWorldProperties(
    const std::string& host_id,
    std::optional<std::string> csp,
    bool enable_messaging) {
  auto& pending_info = pending_worlds_info_[host_id];
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

  for (const auto& [world_id, world_info] : isolated_worlds_) {
    if (world_info.host_id == host_id &&
        world_info.execution_world == mojom::ExecutionWorld::kUserScript) {
      blink::WebIsolatedWorldInfo info;
      info.security_origin = blink::WebSecurityOrigin::Create(world_info.url);
      info.human_readable_name = blink::WebString::FromUTF8(world_info.name);
      info.stable_id = blink::WebString::FromUTF8(host_id);
      info.content_security_policy =
          blink::WebString::FromUTF8(*pending_info.csp);
      blink::SetIsolatedWorldInfo(world_id, info);
    }
  }
}

bool IsolatedWorldManager::IsMessagingEnabledInUserScriptWorlds(
    const std::string& host_id) {
  auto iter = pending_worlds_info_.find(host_id);
  return iter != pending_worlds_info_.end() && iter->second.enable_messaging;
}

int IsolatedWorldManager::GetOrCreateIsolatedWorldForHost(
    const InjectionHost& injection_host,
    mojom::ExecutionWorld execution_world) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static_assert(
      static_cast<int>(mojom::ExecutionWorld::kMaxValue) == 2,
      "You've added a new execution world! Does this code need to be updated?");
  CHECK_NE(mojom::ExecutionWorld::kMain, execution_world);

  static int g_next_isolated_world_id =
      ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId();

  int world_id = 0;
  IsolatedWorldInfo* world_info = nullptr;
  const std::string& host_id = injection_host.id().id;

  auto iter = base::ranges::find_if(
      isolated_worlds_, [host_id, execution_world](const auto& entry) {
        return entry.second.host_id == host_id &&
               entry.second.execution_world == execution_world;
      });

  if (iter != isolated_worlds_.end()) {
    world_id = iter->first;
    world_info = &iter->second;
  } else {
    world_id = g_next_isolated_world_id++;
    // This map will tend to pile up over time, but realistically, you're never
    // going to have enough injection hosts for it to matter.
    // TODO(crbug/1429408): Are we sure about that? Processes can stick around
    // awhile.... (and this could be affected by introducing user script
    // worlds).
    world_info = &isolated_worlds_[world_id];
    world_info->host_id = host_id;
    world_info->execution_world = execution_world;
    world_info->url = injection_host.url();
    world_info->name = injection_host.name();
  }

  // Initialize CSP for the new world. First, check if we have a dedicated
  // user script CSP.
  const std::string* csp = nullptr;
  if (execution_world == mojom::ExecutionWorld::kUserScript) {
    auto user_script_iter = pending_worlds_info_.find(host_id);
    if (user_script_iter != pending_worlds_info_.end() &&
        user_script_iter->second.csp) {
      csp = &(*user_script_iter->second.csp);
    }
  }

  // If not, check the injection host's default CSP.
  if (!csp) {
    csp = injection_host.GetContentSecurityPolicy();
  }

  // If we found a CSP, apply it to the world. Otherwise, explicitly clear out
  // any old CSP.
  world_info->csp = csp ? std::optional<std::string>(*csp) : std::nullopt;

  // Even though there may be an existing world for this `injection_host`'s id,
  // the properties may have changed (e.g. due to an extension update).
  // Overwrite any existing entries.
  UpdateBlinkIsolatedWorldInfo(world_id, *world_info);

  return world_id;
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
