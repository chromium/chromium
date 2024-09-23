// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/network_permissions_updater.h"

#include "base/barrier_closure.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cors_origin_pattern_setter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace extensions {

namespace {

void SetCorsOriginAccessListForExtensionHelper(
    const std::vector<content::BrowserContext*>& browser_contexts,
    const Extension& extension,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  auto barrier_closure =
      BarrierClosure(browser_contexts.size(), std::move(closure));
  for (content::BrowserContext* browser_context : browser_contexts) {
    // SetCorsOriginAccessListForExtensionHelper should only affect an incognito
    // profile if the extension is actually allowed to run in an incognito
    // profile (not just by the extension manifest, but also by user
    // preferences).
    if (browser_context->IsOffTheRecord()) {
      DCHECK(util::IsIncognitoEnabled(extension.id(), browser_context));
    }

    content::CorsOriginPatternSetter::Set(
        browser_context, extension.origin(), mojo::Clone(allow_patterns),
        mojo::Clone(block_patterns), barrier_closure);
  }
}

}  // namespace

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
    ContextSet context_set,
    base::OnceClosure completion_callback) {
  auto updater = std::make_unique<NetworkPermissionsUpdater>(
      PassKey(), browser_context, std::move(completion_callback));
  auto* updater_raw = updater.get();

  // The callback takes ownership of `updater`, ensuring it's deleted when
  // the update completes.
  updater_raw->UpdateExtension(
      extension, context_set,
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

  // When updating all extensions, we always use "all related contexts".
  constexpr ContextSet kContextSet = ContextSet::kAllRelatedContexts;

  for (const auto& extension : extensions) {
    updater_raw->UpdateExtension(*extension, kContextSet, barrier_closure);
  }
}

void NetworkPermissionsUpdater::ResetOriginAccessForExtension(
    content::BrowserContext& browser_context,
    const Extension& extension) {
  SetCorsOriginAccessListForExtensionHelper({&browser_context}, extension, {},
                                            {}, base::DoNothing());
}

void NetworkPermissionsUpdater::UpdateExtension(
    const Extension& extension,
    ContextSet context_set,
    base::OnceClosure completion_callback) {
  std::vector<content::BrowserContext*> target_contexts;
  if (context_set == ContextSet::kCurrentContextOnly) {
    target_contexts = {browser_context_.get()};
  } else {
    DCHECK_EQ(ContextSet::kAllRelatedContexts, context_set);
    target_contexts =
        ExtensionsBrowserClient::Get()->GetRelatedContextsForExtension(
            browser_context_, extension);
  }

  SetCorsOriginAccessListForExtensionHelper(
      target_contexts, extension, CreateCorsOriginAccessAllowList(extension),
      CreateCorsOriginAccessBlockList(extension),
      std::move(completion_callback));
}

// static
void NetworkPermissionsUpdater::OnOriginAccessUpdated(
    std::unique_ptr<NetworkPermissionsUpdater> updater) {
  std::move(updater->completion_callback_).Run();
}

}  // namespace extensions
