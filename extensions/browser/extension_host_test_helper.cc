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

void ExtensionHostTestHelper::OnExtensionHostDestroyed(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  EventSeen(host, HostEvent::kDestroyed);
}

void ExtensionHostTestHelper::WaitFor(HostEvent event) {
  DCHECK(!waiting_for_);

  if (base::Contains(observed_events_, event))
    return;

  base::RunLoop run_loop;
  // Note: We use QuitWhenIdle (instead of Quit) so that any other listeners of
  // the relevant events get a chance to run first.
  quit_loop_ = run_loop.QuitWhenIdleClosure();
  waiting_for_ = event;
  run_loop.Run();
}

void ExtensionHostTestHelper::EventSeen(ExtensionHost* host, HostEvent event) {
  // Check if the host matches our restrictions.
  if (!extension_id_.empty() && host->extension_id() != extension_id_)
    return;

  observed_events_.insert(event);
  if (waiting_for_ == event) {
    DCHECK(quit_loop_);
    waiting_for_.reset();
    std::move(quit_loop_).Run();
  }
}

}  // namespace extensions
