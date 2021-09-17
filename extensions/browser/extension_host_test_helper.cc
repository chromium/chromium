// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host_test_helper.h"

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
    : extension_id_(std::move(extension_id)) {
  host_registry_observation_.Observe(
      ExtensionHostRegistry::Get(browser_context));
}

ExtensionHostTestHelper::~ExtensionHostTestHelper() = default;

void ExtensionHostTestHelper::OnExtensionHostCreated(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  EventSeen(host, HostEvent::kCreated);
}

void ExtensionHostTestHelper::OnExtensionHostDestroyed(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  EventSeen(host, HostEvent::kDestroyed);
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
  if (!extension_id_.empty() && host->extension_id() != extension_id_)
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
