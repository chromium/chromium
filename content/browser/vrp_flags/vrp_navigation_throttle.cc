// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vrp_flags/vrp_navigation_throttle.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "url/gurl.h"

namespace content {

namespace {

using RegistryMap =
    base::flat_map<uint16_t, mojo::PendingReceiver<vrp_flags::mojom::VrpFlags>>;

// Returns the UI-thread registry mapping unique victim ports to pending
// VrpFlags receivers.
RegistryMap& GetRegistry() {
  static base::NoDestructor<RegistryMap> registry;
  return *registry;
}

}  // namespace

// static
void VrpNavigationThrottle::RegisterPortReceiver(
    uint16_t port,
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) {
  // Store the pending receiver associated with the given port.
  GetRegistry()[port] = std::move(receiver);
}

// static
void VrpNavigationThrottle::MaybeCreateAndAdd(
    NavigationThrottleRegistry& registry) {
  registry.AddThrottle(std::make_unique<VrpNavigationThrottle>(registry));
}

VrpNavigationThrottle::VrpNavigationThrottle(
    NavigationThrottleRegistry& registry)
    : NavigationThrottle(registry) {}

VrpNavigationThrottle::~VrpNavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
VrpNavigationThrottle::WillProcessResponse() {
  const GURL& url = navigation_handle()->GetURL();

  // Only intercept navigations targeting victim.test or localhost.
  if (url.host() != "victim.test" && url.host() != "127.0.0.1") {
    return PROCEED;
  }

  int int_port = url.EffectiveIntPort();
  if (int_port < 0 || int_port > 65535) {
    return PROCEED;
  }

  uint16_t port = static_cast<uint16_t>(int_port);
  auto& registry = GetRegistry();
  auto it = registry.find(port);
  if (it == registry.end()) {
    return PROCEED;
  }

  // Extract the receiver and erase from registry to ensure single-use binding.
  mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver =
      std::move(it->second);
  registry.erase(it);

  if (receiver) {
    RenderProcessHost* process =
        navigation_handle()->GetRenderFrameHost()->GetProcess();
    process->BindReceiver(std::move(receiver));
  }

  return PROCEED;
}

const char* VrpNavigationThrottle::GetNameForLogging() {
  return "VrpNavigationThrottle";
}

}  // namespace content
