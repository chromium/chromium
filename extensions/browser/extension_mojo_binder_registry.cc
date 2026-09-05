// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_mojo_binder_registry.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/map_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "mojo/public/cpp/bindings/binder_map.h"

class AimEligibilityExtensionBinderProvider;

namespace contextual_tasks {
class ContextualTasksExtensionBinderProvider;
}

namespace extensions {

class ExtensionMojoBinderRegistryTest;
class RendererStartupHelperTest;
class ServiceWorkerTest;

ExtensionMojoBinderProvider::ExtensionMojoBinderProvider(
    ExtensionId extension_id)
    : extension_id_(std::move(extension_id)) {}

ExtensionMojoBinderProvider::~ExtensionMojoBinderProvider() = default;

bool ExtensionMojoBinderProvider::IsMojoJsEnabledForFrame() const {
  return false;
}

bool ExtensionMojoBinderProvider::IsMojoJsEnabledForServiceWorker() const {
  return false;
}

ExtensionMojoBinderRegistry::ExtensionMojoBinderRegistry() = default;

ExtensionMojoBinderRegistry::~ExtensionMojoBinderRegistry() = default;

// These explicit specializations effectively allow exposing additional mojo
// interfaces to different renderers. Thus, additions to these specializations
// require review from IPC_SECURITY_OWNERS. To enforce this via presubmit,
// binder provider files files must be named *_extension_binder_provider.h/cc.
template <>
void ExtensionMojoBinderRegistry::RegisterProvider(
    base::PassKey<AimEligibilityExtensionBinderProvider>,
    std::unique_ptr<ExtensionMojoBinderProvider> provider) {
  RegisterProviderImpl(std::move(provider));
}

template <>
void ExtensionMojoBinderRegistry::RegisterProvider(
    base::PassKey<contextual_tasks::ContextualTasksExtensionBinderProvider>,
    std::unique_ptr<ExtensionMojoBinderProvider> provider) {
  RegisterProviderImpl(std::move(provider));
}

template <>
void ExtensionMojoBinderRegistry::RegisterProvider(
    base::PassKey<ExtensionMojoBinderRegistryTest>,
    std::unique_ptr<ExtensionMojoBinderProvider> provider) {
  RegisterProviderImpl(std::move(provider));
}

template <>
void ExtensionMojoBinderRegistry::RegisterProvider(
    base::PassKey<RendererStartupHelperTest>,
    std::unique_ptr<ExtensionMojoBinderProvider> provider) {
  RegisterProviderImpl(std::move(provider));
}

template <>
void ExtensionMojoBinderRegistry::RegisterProvider(
    base::PassKey<ServiceWorkerTest>,
    std::unique_ptr<ExtensionMojoBinderProvider> provider) {
  RegisterProviderImpl(std::move(provider));
}

void ExtensionMojoBinderRegistry::RegisterProviderImpl(
    std::unique_ptr<ExtensionMojoBinderProvider> provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(provider);
  ExtensionId extension_id = provider->extension_id();
  auto [it, inserted] =
      providers_.insert({std::move(extension_id), std::move(provider)});
  CHECK(inserted) << "A provider for component extension '" << it->first
                  << "' is already registered.";
}

void ExtensionMojoBinderRegistry::PopulateFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension& extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(binder_map);
  ExtensionMojoBinderProvider* provider = GetProviderIfAllowed(extension);
  if (!provider) {
    return;
  }
  if (render_frame_host &&
      util::GetExtensionIdForSiteInstance(
          *render_frame_host->GetSiteInstance()) != extension.id()) {
    return;
  }
  provider->PopulateFrameBinders(*binder_map, render_frame_host, extension);
}

void ExtensionMojoBinderRegistry::PopulateServiceWorkerBinders(
    mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
        binder_map,
    content::BrowserContext* browser_context,
    const Extension& extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(binder_map);
  ExtensionMojoBinderProvider* provider = GetProviderIfAllowed(extension);
  if (!provider) {
    return;
  }
  provider->PopulateServiceWorkerBinders(*binder_map, browser_context,
                                         extension);
}

bool ExtensionMojoBinderRegistry::IsMojoJsEnabledForFrame(
    const Extension& extension) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExtensionMojoBinderProvider* provider = GetProviderIfAllowed(extension);
  return provider && provider->IsMojoJsEnabledForFrame();
}

bool ExtensionMojoBinderRegistry::IsMojoJsEnabledForServiceWorker(
    const Extension& extension) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExtensionMojoBinderProvider* provider = GetProviderIfAllowed(extension);
  return provider && provider->IsMojoJsEnabledForServiceWorker();
}

void ExtensionMojoBinderRegistry::ClearProvidersForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  providers_.clear();
}

ExtensionMojoBinderProvider* ExtensionMojoBinderRegistry::GetProviderIfAllowed(
    const Extension& extension) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!Manifest::IsComponentLocation(extension.location())) {
    return nullptr;
  }
  return base::FindPtrOrNull(providers_, extension.id());
}

}  // namespace extensions
