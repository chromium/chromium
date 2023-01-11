// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_browser_context.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/shell/browser/shell_special_storage_policy.h"

namespace extensions {

// Create a normal recording browser context. If we used an incognito context
// then app_shell would also have to create a normal context and manage both.
ShellBrowserContext::ShellBrowserContext()
    : content::ShellBrowserContext(false /* off_the_record */,
                                   true /* delay_services_creation */),
      storage_policy_(base::MakeRefCounted<ShellSpecialStoragePolicy>()) {}

ShellBrowserContext::~ShellBrowserContext() {
  NotifyWillBeDestroyed();
}

content::BrowserPluginGuestManager* ShellBrowserContext::GetGuestManager() {
  return guest_view::GuestViewManager::FromBrowserContext(this);
}

storage::SpecialStoragePolicy* ShellBrowserContext::GetSpecialStoragePolicy() {
  return storage_policy_.get();
}

}  // namespace extensions
