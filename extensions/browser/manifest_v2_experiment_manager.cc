// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/manifest_v2_experiment_manager.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "base/one_shot_event.h"
#include "base/strings/stringprintf.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_types.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Whether to override the MV2 deprecation for testing purposes.
bool g_allow_mv2_for_testing = false;

// Returns the suffix to use for histograms related to the manifest location
// grouping.
const char* GetHistogramManifestLocation(mojom::ManifestLocation location) {
  switch (location) {
    case mojom::ManifestLocation::kComponent:
    case mojom::ManifestLocation::kExternalComponent:
      return "Component";
    case mojom::ManifestLocation::kExternalPolicy:
    case mojom::ManifestLocation::kExternalPolicyDownload:
      return "Policy";
    case mojom::ManifestLocation::kCommandLine:
    case mojom::ManifestLocation::kUnpacked:
      return "Unpacked";
    case mojom::ManifestLocation::kExternalRegistry:
    case mojom::ManifestLocation::kExternalPref:
    case mojom::ManifestLocation::kExternalPrefDownload:
      return "External";
    case mojom::ManifestLocation::kInternal:
      return "Internal";
    case mojom::ManifestLocation::kInvalidLocation:
      NOTREACHED();
  }
}

class ManifestV2ExperimentManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  ManifestV2ExperimentManagerFactory();
  ManifestV2ExperimentManagerFactory(
      const ManifestV2ExperimentManagerFactory&) = delete;
  ManifestV2ExperimentManagerFactory& operator=(
      const ManifestV2ExperimentManagerFactory&) = delete;
  ~ManifestV2ExperimentManagerFactory() override = default;

  ManifestV2ExperimentManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

ManifestV2ExperimentManagerFactory::ManifestV2ExperimentManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ManifestV2ExperimentManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

ManifestV2ExperimentManager*
ManifestV2ExperimentManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ManifestV2ExperimentManager*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext*
ManifestV2ExperimentManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  return ExtensionsBrowserClient::Get()
      ->GetContextRedirectedToOriginalWithoutAshInternals(browser_context);
}

std::unique_ptr<KeyedService>
ManifestV2ExperimentManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ManifestV2ExperimentManager>(context);
}

bool ManifestV2ExperimentManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

// Returns true if legacy extensions should be disabled, looking at both
// experiment stage and global state.
bool ShouldDisableLegacyExtensions() {
  if (g_allow_mv2_for_testing) {
    // We allow legacy MV2 extensions for testing purposes.
    return false;
  }

  return true;
}

}  // namespace

ManifestV2ExperimentManager::ManifestV2ExperimentManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  registry_observation_.Observe(ExtensionRegistry::Get(browser_context));

  ExtensionSystem::Get(browser_context)
      ->ready()
      .Post(FROM_HERE,
            base::BindOnce(&ManifestV2ExperimentManager::OnExtensionSystemReady,
                           weak_factory_.GetWeakPtr()));
}

ManifestV2ExperimentManager::~ManifestV2ExperimentManager() = default;

// static
ManifestV2ExperimentManager* ManifestV2ExperimentManager::Get(
    content::BrowserContext* browser_context) {
  return static_cast<ManifestV2ExperimentManagerFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* ManifestV2ExperimentManager::GetFactory() {
  static base::NoDestructor<ManifestV2ExperimentManagerFactory> g_factory;
  return g_factory.get();
}

bool ManifestV2ExperimentManager::IsExtensionAffected(
    const Extension& extension) {
  return impact_checker_.IsExtensionAffected(extension);
}

bool ManifestV2ExperimentManager::ShouldBlockExtensionInstallation(
    int manifest_version,
    Manifest::Type manifest_type,
    mojom::ManifestLocation manifest_location) {
  if (!ShouldDisableLegacyExtensions()) {
    return false;
  }

  // Otherwise, if the extension is affected by the deprecation, it should be
  // blocked.
  return impact_checker_.IsExtensionAffected(manifest_version, manifest_type,
                                             manifest_location);
}

bool ManifestV2ExperimentManager::ShouldBlockExtensionEnable(
    const Extension& extension) {
  if (!ShouldDisableLegacyExtensions()) {
    return false;
  }

  return impact_checker_.IsExtensionAffected(
      extension.manifest_version(), extension.GetType(), extension.location());
}

bool ManifestV2ExperimentManager::DidUserAcknowledgeNoticeGlobally() {
  return extension_prefs()->GetPrefAsBoolean(
      kMV2DeprecationUnsupportedAcknowledgedGloballyPref);
}

void ManifestV2ExperimentManager::MarkNoticeAsAcknowledgedGlobally() {
  extension_prefs()->SetBooleanPref(
      kMV2DeprecationUnsupportedAcknowledgedGloballyPref, true);
}

ExtensionPrefs* ManifestV2ExperimentManager::extension_prefs() {
  if (!extension_prefs_) {
    extension_prefs_ = ExtensionPrefs::Get(browser_context_);
  }
  return extension_prefs_;
}

void ManifestV2ExperimentManager::OnExtensionSystemReady() {
  CheckDisabledExtensions();
  DisableAffectedExtensions();

  EmitMetricsForProfileReady();
}

void ManifestV2ExperimentManager::DisableAffectedExtensions() {
  if (!ShouldDisableLegacyExtensions()) {
    return;
  }

  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context_);
  std::set<scoped_refptr<const Extension>> extensions_to_disable;

  // Disable all applicable MV2 extensions.
  for (const auto& extension : extension_registry->enabled_extensions()) {
    if (!impact_checker_.IsExtensionAffected(*extension)) {
      continue;
    }

    extensions_to_disable.insert(extension);
  }

  auto* registrar = ExtensionRegistrar::Get(browser_context_);
  for (const auto& extension : extensions_to_disable) {
    registrar->DisableExtension(
        extension->id(),
        {disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION});
  }
}

void ManifestV2ExperimentManager::CheckDisabledExtensions() {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context_);
  ExtensionSet disabled_extensions;
  // Loop through all disabled extensions. For each, check if they should be
  // re-enabled (e.g. because they've updated to MV3 or a change in policy
  // settings).
  // Use a copy of the set to avoid changing the set while iterating.
  disabled_extensions.InsertAll(extension_registry->disabled_extensions());
  for (const auto& extension : disabled_extensions) {
    MaybeReEnableExtension(*extension);
  }
}

void ManifestV2ExperimentManager::MaybeReEnableExtension(
    const Extension& extension) {
  if (!extension_prefs()->HasDisableReason(
          extension.id(),
          disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION)) {
    // We only care about extensions that were disabled for this reason.
    return;
  }

  // Check if the extension is still affected *and* whether the environment is
  // still one in which extensions should be disabled. It's possible the user
  // moved from a later experiment stage to an earlier one or set a feature
  // flag, in which case extensions should be re-enabled.
  if (impact_checker_.IsExtensionAffected(extension) &&
      ShouldDisableLegacyExtensions()) {
    return;
  }

  // Remove the disable reason (possibly re-enabling the extension).
  ExtensionRegistrar::Get(browser_context_)
      ->RemoveDisableReasonAndMaybeEnable(
          extension.id(), disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION);
}

void ManifestV2ExperimentManager::EmitMetricsForProfileReady() {
  if (!ShouldDisableLegacyExtensions()) {
    // Don't bother reporting MV2-specific metrics if the user isn't in an
    // environment in which extensions could be disabled.
    return;
  }

  if (!ExtensionsBrowserClient::Get()->CanUseNonComponentExtensions(
          browser_context_)) {
    // Don't report metrics if the user can't install extensions in this
    // profile.
    return;
  }

  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context_);

  auto emit_state_for_mv2_extension = [this](const Extension& extension,
                                             bool is_enabled) {
    if (extension.manifest_version() != 2) {
      return;
    }

    if (extension.GetType() != Manifest::Type::kExtension &&
        extension.GetType() != Manifest::Type::kLoginScreenExtension) {
      return;
    }

    MV2ExtensionState extension_state = MV2ExtensionState::kUnaffected;
    if (!impact_checker_.IsExtensionAffected(extension)) {
      extension_state = MV2ExtensionState::kUnaffected;
    } else if (extension_prefs()->HasDisableReason(
                   extension.id(),
                   disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION)) {
      CHECK(!is_enabled);
      extension_state = MV2ExtensionState::kHardDisabled;
    } else {
      extension_state = MV2ExtensionState::kOther;
    }

    std::string histogram_name =
        base::StringPrintf("Extensions.MV2Deprecation.MV2ExtensionState.%s",
                           GetHistogramManifestLocation(extension.location()));

    base::UmaHistogramEnumeration(histogram_name, extension_state);
  };

  for (const auto& extension : extension_registry->enabled_extensions()) {
    emit_state_for_mv2_extension(*extension, /*is_enabled=*/true);
  }
  for (const auto& extension : extension_registry->disabled_extensions()) {
    emit_state_for_mv2_extension(*extension, /*is_enabled=*/false);
  }
}

void ManifestV2ExperimentManager::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  if (!is_update) {
    // We would only ever re-enable a disabled extension if it was already
    // installed. No need to look at new installs.
    return;
  }

  MaybeReEnableExtension(*extension);
}

void ManifestV2ExperimentManager::
    DisableAffectedExtensionsForTesting() {  // IN-TEST
  DisableAffectedExtensions();
}

void ManifestV2ExperimentManager::
    EmitMetricsForProfileReadyForTesting() {  // IN-TEST
  EmitMetricsForProfileReady();
}

base::AutoReset<bool>
ManifestV2ExperimentManager::AllowMV2ExtensionsForTesting(  // IN-TEST
    base::PassKey<ScopedTestMV2Enabler> pass_key) {
  return base::AutoReset<bool>(&g_allow_mv2_for_testing, true);
}

}  // namespace extensions
