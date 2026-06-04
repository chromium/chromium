// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MANIFEST_V2_EXPERIMENT_MANAGER_H_
#define EXTENSIONS_BROWSER_MANIFEST_V2_EXPERIMENT_MANAGER_H_

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/mv2_deprecation_impact_checker.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionPrefs;
class ScopedTestMV2Enabler;
enum class MV2ExperimentStage;

// The central class responsible for managing experiments related to the MV2
// deprecation.
class ManifestV2ExperimentManager : public KeyedService,
                                    public ExtensionRegistryObserver {
 public:
  explicit ManifestV2ExperimentManager(
      content::BrowserContext* browser_context);
  ManifestV2ExperimentManager(const ManifestV2ExperimentManager&) = delete;
  ManifestV2ExperimentManager& operator=(const ManifestV2ExperimentManager&) =
      delete;
  ~ManifestV2ExperimentManager() override;

  // The possible states for an MV2 extension during the experiments.
  // Do not re-order entries, as these are used in histograms.
  // Exposed for testing purposes.
  enum class MV2ExtensionState {
    // Extension is unaffected by the MV2 deprecation (e.g., it's a policy-
    // installed extension with the proper enterprise policies set).
    kUnaffected = 0,
    // The extension was disabled by Chrome, but may be re-enabled by the user.
    kSoftDisabled = 1,
    // The extension was previously disabled by Chrome, but was re-enabled by
    // the user.
    kUserReEnabled = 2,
    // Any other state. This includes e.g. extensions that are disabled, but for
    // other reasons.
    kOther = 3,
    // The extension is disabled, and may not be re-enabled by the user.
    kHardDisabled = 4,
    kMaxValue = kHardDisabled,
  };

  // Retrieves the ManifestV2ExperimentManager associated with the given
  // `browser_context`. Note this instance is shared between on- and off-the-
  // record contexts.
  static ManifestV2ExperimentManager* Get(
      content::BrowserContext* browser_context);

  // Returns the singleton instance of the factory for this KeyedService.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns true if the given `extension` is affected by the MV2 deprecation.
  // This may be false if, e.g., the extension is policy-installed.
  bool IsExtensionAffected(const Extension& extension);

  // Returns true if a new installation with the given `manifest_version`,
  // `manifest_type`, and `manifest_location` should be blocked.
  bool ShouldBlockExtensionInstallation(
      int manifest_version,
      Manifest::Type manifest_type,
      mojom::ManifestLocation manifest_location);

  // Returns true if Chrome should disallow enabling the given `extension`.
  bool ShouldBlockExtensionEnable(const Extension& extension);

  // Returns true if the user has acknowledge the notice during the current MV2
  // deprecation `experiment_stage_`.
  bool DidUserAcknowledgeNoticeGlobally();

  // Called to indicate the user chose to acknowledge the global notice during
  // the current MV2 deprecation `experiment_stage_`..
  void MarkNoticeAsAcknowledgedGlobally();

  // Registers `callback` to run when this has finished its initialization
  // steps. `is_manager_ready_` must be false for this to be called.
  base::CallbackListSubscription RegisterOnManagerReadyCallback(
      base::RepeatingClosure callback);

  // Whether the disabled dialog has been triggered for this `browser_context_`.
  bool has_triggered_disabled_dialog() {
    return has_triggered_disabled_dialog_;
  }
  // This should be called when a new window is opened for `browser_context_`.
  void SetHasTriggeredDisabledDialog(bool has_triggered);

  // Returns whether this has finished its initialization steps.
  bool is_manager_ready() { return is_manager_ready_; }

  // Helpers to call internal methods directly for testing purposes. These are
  // useful to have an extension that's installed in the body of a test get
  // disabled, since this normally only happens on startup.
  void DisableAffectedExtensionsForTesting();
  void EmitMetricsForProfileReadyForTesting();

  // See ScopedTestMV2Enabler for details.
  static base::AutoReset<bool> AllowMV2ExtensionsForTesting(
      base::PassKey<ScopedTestMV2Enabler> pass_key);

 private:
  // Lazily initialize and access `extension_prefs_`. We do this lazily because:
  // - This service is created on Profile creation.
  // - A bunch of unit tests override ExtensionPrefs after Profile creation, but
  //   before the "real" test starts.
  // As such, if we instantiated ExtensionPrefs in the constructor, it would be
  // the improper ExtensionPrefs object and would trigger raw_ptr violations.
  ExtensionPrefs* extension_prefs();

  // Called when the extension system has finished its initialization steps.
  void OnExtensionSystemReady();

  // Disables any Manifest V2 extensions that are affected by the experiment,
  // if the user hasn't chosen to re-enable them.
  void DisableAffectedExtensions();

  // Loops through disabled extensions and checks if any should be re-enabled.
  void CheckDisabledExtensions();

  // Re-enables the `extension` if it should no longer be disabled by the MV2
  // deprecation (e.g., if it updated to MV3).
  void MaybeReEnableExtension(const Extension& extension);

  // Emits metrics about the state of installed extensions related to the
  // MV2 deprecation.
  void EmitMetricsForProfileReady();

  // ExtensionRegistry:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;

  // A helper object to determine if a given extension is affected by the
  // MV2 deprecation experiments.
  MV2DeprecationImpactChecker impact_checker_;

  // The associated ExtensionPrefs. Guaranteed to be safe to use since this
  // class depends upon them via the KeyedService infrastructure.
  raw_ptr<ExtensionPrefs> extension_prefs_;

  // The associated BrowserContext. Guaranteed to be safe to use since this is
  // a KeyedService for the context.
  raw_ptr<content::BrowserContext> browser_context_;

  // Whether the disabled dialog has been triggered for this `browser_context_`.
  bool has_triggered_disabled_dialog_ = false;

  // Whether this class has finished its initialization steps.
  bool is_manager_ready_ = false;

  // Callback to be run when this has finished its initialization steps.
  base::RepeatingCallbackList<void()> on_manager_ready_callback_list_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  base::WeakPtrFactory<ManifestV2ExperimentManager> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MANIFEST_V2_EXPERIMENT_MANAGER_H_
