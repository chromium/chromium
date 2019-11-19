// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_browser_context.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/shell/browser/shell_special_storage_policy.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"

namespace extensions {

// Create a normal recording browser context. If we used an incognito context
// then app_shell would also have to create a normal context and manage both.
ShellBrowserContext::ShellBrowserContext()
    : content::ShellBrowserContext(false /* off_the_record */,
                                   true /* delay_services_creation */),
      storage_policy_(new ShellSpecialStoragePolicy) {}

ShellBrowserContext::~ShellBrowserContext() {
  content::BrowserContext::NotifyWillBeDestroyed(this);
}

content::BrowserPluginGuestManager* ShellBrowserContext::GetGuestManager() {
  return guest_view::GuestViewManager::FromBrowserContext(this);
}

storage::SpecialStoragePolicy* ShellBrowserContext::GetSpecialStoragePolicy() {
  return storage_policy_.get();
}

void ShellBrowserContext::SetCorsOriginAccessListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  // This method is called for Extension supports, but tests do not need to
  // support exceptional CORS handling.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(closure));
}

}  // namespace extensions
