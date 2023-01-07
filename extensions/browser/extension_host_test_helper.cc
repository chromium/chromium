// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host_test_helper.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "extensions/browser/extension_host.h"

namespace extensions {

ExtensionHostTestHelper::ExtensionHostTestHelper(
    content::BrowserContext* browser_context)
    : ExtensionHostTestHelper(browser_context, ExtensionId()) {}

ExtensionHostTestHelper::ExtensionHostTestHelper(
    content::BrowserContext* browser_context,
    ExtensionId extension_id)
    : browser_context_(browser_context),
      extension_id_(std::move(extension_id)) {
  host_registry_observation_.Observe(
      ExtensionHostRegistry::Get(browser_context));
}

ExtensionHostTestHelper::~ExtensionHostTestHelper() = default;

void ExtensionHostTestHelper::RestrictToType(mojom::ViewType type) {
  // Restricting to both a specific host and a type is either redundant (if
  // the types match) or contradictory (if they don't). Don't allow it.
  DCHECK(!restrict_to_host_) << "Can't restrict to both a host and view type.";
  restrict_to_type_ = type;
}

void ExtensionHostTestHelper::RestrictToHost(const ExtensionHost* host) {
  // Restricting to both a specific host and a type is either redundant (if
  // the types match) or contradictory (if they don't). Don't allow it.
  DCHECK(!restrict_to_type_) << "Can't restrict to both a host and view type.";
  restrict_to_host_ = host;
}

void ExtensionHostTestHelper::OnExtensionHostRenderProcessReady(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  EventSeen(host, HostEvent::kRenderProcessReady);
}

void ExtensionHostTestHelper::OnExtensionHostDocumentElementAvailable(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  EventSeen(host, HostEvent::kDocumentElementAvailable);
}

void ExtensionHostTestHelper::OnExtensionHostCompletedFirstLoad(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  EventSeen(host, HostEvent::kCompletedFirstLoad);
}

void ExtensionHostTestHelper::OnExtensionHostDestroyed(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  EventSeen(host, HostEvent::kDestroyed);
}

void ExtensionHostTestHelper::OnExtensionHostRenderProcessGone(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  EventSeen(host, HostEvent::kRenderProcessGone);
}

ExtensionHost* ExtensionHostTestHelper::WaitFor(HostEvent event) {
  DCHECK(!waiting_for_);

  auto iter = observed_events_.find(event);
  if (iter != observed_events_.end()) {
    // Note: This can be null if the host has been destroyed.
    return iter->second;
  }

  base::RunLoop run_loop;
  // Note: We use QuitWhenIdle (instead of Quit) so that any other listeners of
  // the relevant events get a chance to run first.
  quit_loop_ = run_loop.QuitWhenIdleClosure();
  waiting_for_ = event;
  run_loop.Run();

  DCHECK(base::Contains(observed_events_, event));
  // Note: This can still be null here if the corresponding ExtensionHost was
  // destroyed.  This is always true when waiting for
  // OnExtensionHostDestroyed(), but can also happen if the ExtensionHost is
  // destroyed while waiting for the run loop to idle.
  return observed_events_[event];
}

void ExtensionHostTestHelper::EventSeen(ExtensionHost* host, HostEvent event) {
  // Check if the host matches our restrictions.
  // Note: We have to check the browser context explicitly because the
  // ExtensionHostRegistry is shared between on- and off-the-record profiles,
  // so the `host`'s browser context may not be the same as the one associated
  // with this object in the case of split mode extensions.
  if (host->browser_context() != browser_context_)
    return;
  if (!extension_id_.empty() && host->extension_id() != extension_id_)
    return;
  if (restrict_to_type_ && host->extension_host_type() != restrict_to_type_)
    return;
  if (restrict_to_host_ && host != restrict_to_host_)
    return;

  if (event == HostEvent::kDestroyed) {
    // Clean up all old pointers to the ExtensionHost on its destruction.
    for (auto& kv : observed_events_) {
      if (kv.second == host)
        kv.second = nullptr;
    }

    // Ensure we don't put a new pointer for the host into the map.
    host = nullptr;
  }

  observed_events_[event] = host;

  if (waiting_for_ == event) {
    DCHECK(quit_loop_);
    waiting_for_.reset();
    std::move(quit_loop_).Run();
  }
}

}  // namespace extensions
