// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VRP_FLAGS_VRP_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_VRP_FLAGS_VRP_NAVIGATION_THROTTLE_H_

#include "components/vrp_flags/vrp_flags.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class NavigationThrottleRegistry;

// NavigationThrottle that intercepts navigations to registered victim target
// ports (e.g. victim.test:<port>) to connect the newly spawned victim renderer
// process to the attacker's VrpFlags Mojo receiver.
class CONTENT_EXPORT VrpNavigationThrottle : public NavigationThrottle {
 public:
  // Registers a pending VrpFlags Mojo receiver associated with a unique port
  // number.
  static void RegisterPortReceiver(
      uint16_t port,
      mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver);

  // Instantiates and registers a VrpNavigationThrottle for frame navigations.
  static void MaybeCreateAndAdd(NavigationThrottleRegistry& registry);

  explicit VrpNavigationThrottle(NavigationThrottleRegistry& registry);
  ~VrpNavigationThrottle() override;

  VrpNavigationThrottle(const VrpNavigationThrottle&) = delete;
  VrpNavigationThrottle& operator=(const VrpNavigationThrottle&) = delete;

  // NavigationThrottle implementation:
  // Checks if the response URL matches a registered victim port, extracts the
  // Mojo receiver, and binds it to the target RenderProcessHost.
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_VRP_FLAGS_VRP_NAVIGATION_THROTTLE_H_
