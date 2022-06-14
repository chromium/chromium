// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/network_permissions_updater.h"

#include "base/barrier_closure.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace extensions {

NetworkPermissionsUpdater::NetworkPermissionsUpdater(
    PassKey pass_key,
    content::BrowserContext& browser_context,
    base::OnceClosure completion_callback)
    : browser_context_(&browser_context),
      completion_callback_(std::move(completion_callback)) {}

NetworkPermissionsUpdater::~NetworkPermissionsUpdater() = default;

// static
void NetworkPermissionsUpdater::UpdateExtension(
    content::BrowserContext& browser_context,
    const Extension& extension,
    base::OnceClosure completion_callback) {
  auto updater = std::make_unique<NetworkPermissionsUpdater>(
      PassKey(), browser_context, std::move(completion_callback));
  auto* updater_raw = updater.get();

  // The callback takes ownership of `updater`, ensuring it's deleted when
  // the update completes.
  updater_raw->UpdateExtension(
      extension,
      base::BindOnce(&NetworkPermissionsUpdater::OnOriginAccessUpdated,
                     std::move(updater)));
}

// static
void NetworkPermissionsUpdater::UpdateAllExtensions(
    content::BrowserContext& browser_context,
    base::OnceClosure completion_callback) {
  auto updater = std::make_unique<NetworkPermissionsUpdater>(
      PassKey(), browser_context, std::move(completion_callback));
  auto* updater_raw = updater.get();

  const ExtensionSet& extensions =
      ExtensionRegistry::Get(&browser_context)->enabled_extensions();

  // The `barrier_closure` takes ownership of `updater`, ensuring it's deleted
  // when the update completes.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      extensions.size(),
      base::BindOnce(&NetworkPermissionsUpdater::OnOriginAccessUpdated,
                     std::move(updater)));

  for (const auto& extension : extensions)
    updater_raw->UpdateExtension(*extension, barrier_closure);
}

void NetworkPermissionsUpdater::UpdateExtension(
    const Extension& extension,
    base::OnceClosure completion_callback) {
  // Non-tab-specific extension permissions are shared across profiles (even for
  // split-mode extensions), so we update all profiles the extension is enabled
  // for.
  util::SetCorsOriginAccessListForExtension(
      ExtensionsBrowserClient::Get()->GetRelatedContextsForExtension(
          browser_context_, extension),
      extension, std::move(completion_callback));
}

// static
void NetworkPermissionsUpdater::OnOriginAccessUpdated(
    std::unique_ptr<NetworkPermissionsUpdater> updater) {
  std::move(updater->completion_callback_).Run();
}

}  // namespace extensions
