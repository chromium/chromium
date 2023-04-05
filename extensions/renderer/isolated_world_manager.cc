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

absl::optional<mojom::ExecutionWorld>
IsolatedWorldManager::GetExecutionWorldForIsolatedWorld(int world_id) {
  auto iter = isolated_worlds_.find(world_id);
  return iter != isolated_worlds_.end() ? absl::optional<mojom::ExecutionWorld>(
                                              iter->second.execution_world)
                                        : absl::nullopt;
}

void IsolatedWorldManager::RemoveIsolatedWorlds(const std::string& host_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::EraseIf(isolated_worlds_, [&host_id](const auto& entry) {
    return entry.second.host_id == host_id;
  });
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
  const std::string& host_id = injection_host.id().id;

  auto iter = base::ranges::find_if(
      isolated_worlds_, [host_id, execution_world](const auto& entry) {
        return entry.second.host_id == host_id &&
               entry.second.execution_world == execution_world;
      });

  if (iter != isolated_worlds_.end()) {
    world_id = iter->first;
  } else {
    world_id = g_next_isolated_world_id++;
    // This map will tend to pile up over time, but realistically, you're never
    // going to have enough injection hosts for it to matter.
    // TODO(crbug/1429408): Are we sure about that? Processes can stick around
    // awhile.... (and this could be affected by introducing user script
    // worlds).
    IsolatedWorldInfo& info = isolated_worlds_[world_id];
    info.host_id = host_id;
    info.execution_world = execution_world;
  }

  blink::WebIsolatedWorldInfo info;
  info.security_origin = blink::WebSecurityOrigin::Create(injection_host.url());
  info.human_readable_name = blink::WebString::FromUTF8(injection_host.name());
  info.stable_id = blink::WebString::FromUTF8(host_id);

  const std::string* csp = injection_host.GetContentSecurityPolicy();
  if (csp) {
    info.content_security_policy = blink::WebString::FromUTF8(*csp);
  }

  // Even though there may be an existing world for this `injection_host`'s id,
  // the properties may have changed (e.g. due to an extension update).
  // Overwrite any existing entries.
  blink::SetIsolatedWorldInfo(world_id, info);

  return world_id;
}

}  // namespace extensions
