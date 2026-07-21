// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_mojo_binder_registry.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace extensions {

// static
ExtensionMojoBinderRegistry* ExtensionMojoBinderRegistry::GetInstance() {
  static base::NoDestructor<ExtensionMojoBinderRegistry> instance;
  return instance.get();
}

ExtensionMojoBinderRegistry::ExtensionMojoBinderRegistry() = default;

ExtensionMojoBinderRegistry::~ExtensionMojoBinderRegistry() = default;

void ExtensionMojoBinderRegistry::RegisterProvider(
    std::unique_ptr<ExtensionMojoBinderProvider> provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(provider);
  ExtensionId extension_id = provider->GetExtensionId();
  auto [it, inserted] =
      providers_.emplace(std::move(extension_id), std::move(provider));
  DCHECK(inserted) << "A provider for component extension '" << it->first
                   << "' is already registered.";
}

void ExtensionMojoBinderRegistry::PopulateFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!extension || !Manifest::IsComponentLocation(extension->location())) {
    return;
  }
  if (render_frame_host &&
      util::GetExtensionIdForSiteInstance(
          *render_frame_host->GetSiteInstance()) != extension->id()) {
    return;
  }
  auto it = providers_.find(extension->id());
  if (it == providers_.end()) {
    return;
  }
  ExtensionBinderMap<content::RenderFrameHost*> filtered_map(
      binder_map, extension,
      base::BindRepeating(
          &ExtensionMojoBinderRegistry::IsAllowedInterfaceForExtension,
          base::Unretained(this)));
  it->second->PopulateFrameBinders(filtered_map, render_frame_host, extension);
}

void ExtensionMojoBinderRegistry::PopulateServiceWorkerBinders(
    mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
        binder_map,
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!extension || !Manifest::IsComponentLocation(extension->location())) {
    return;
  }
  auto it = providers_.find(extension->id());
  if (it == providers_.end()) {
    return;
  }
  ExtensionBinderMap<const content::ServiceWorkerVersionBaseInfo&> filtered_map(
      binder_map, extension,
      base::BindRepeating(
          &ExtensionMojoBinderRegistry::IsAllowedInterfaceForExtension,
          base::Unretained(this)));
  it->second->PopulateServiceWorkerBinders(filtered_map, browser_context,
                                           extension);
}

bool ExtensionMojoBinderRegistry::IsAllowedInterfaceForExtension(
    const Extension* extension,
    std::string_view interface_name) const {
  if (bypass_allowlist_for_testing_) {
    return true;
  }

  // Additions to this allowlist require a review from IPC_SECURITY_OWNERS.
  static constexpr auto kAllowedComponentExtensionInterfaces =
      base::MakeFixedFlatSet<std::pair<std::string_view, std::string_view>>({
          {extension_misc::kAimEligibilityExtensionId,
           "aim_eligibility.mojom.PageHandlerFactory"},
          {extension_misc::kAimEligibilityExtensionId,
           "color_change_listener.mojom.PageHandler"},
      });
  return kAllowedComponentExtensionInterfaces.contains(
      {extension->id(), interface_name});
}

void ExtensionMojoBinderRegistry::SetBypassAllowlistForTesting(bool bypass) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bypass_allowlist_for_testing_ = bypass;
}

void ExtensionMojoBinderRegistry::ClearProvidersForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  providers_.clear();
}

}  // namespace extensions
