// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webstore_private/extension_install_status.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_management_client.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/managed_installation_mode.h"
#include "extensions/browser/manifest_v2_experiment_manager.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

// A helper function to determine if an extension from web store with given
// information should be blocked by enterprise policy. It checks extension's
// installation mode, permission and manifest type.
// Returns true if the extension |mode| is blocked, removed or allowed by
// wildcard/update_url but blocked by |manifest type| or |required permissions|.
bool IsExtensionInstallBlockedByPolicy(
    ExtensionManagementClient* extension_management_client,
    ManagedInstallationMode mode,
    const ExtensionId& extension_id,
    const std::string& update_url,
    const Manifest::Type manifest_type,
    const PermissionSet& required_permissions) {
  switch (mode) {
    case ManagedInstallationMode::kBlocked:
    case ManagedInstallationMode::kRemoved:
      return true;
    case ManagedInstallationMode::kForced:
    case ManagedInstallationMode::kRecommended:
      return false;
    case ManagedInstallationMode::kAllowed:
      break;
  }

  if (extension_management_client->IsInstallationExplicitlyAllowed(
          extension_id)) {
    return false;
  }

  // Extension is allowed by wildcard or update_url, checks required permissions
  // and manifest type.
  if (manifest_type != Manifest::Type::kUnknown &&
      !extension_management_client->IsAllowedManifestType(manifest_type,
                                                          extension_id)) {
    return true;
  }

  if (!extension_management_client->IsPermissionSetAllowed(
          extension_id, update_url, required_permissions)) {
    return true;
  }

  return false;
}

ExtensionInstallStatus PerformSynchronousChecks(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context,
    Manifest::Type manifest_type,
    const PermissionSet& required_permission_set,
    int manifest_version) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));

  const GURL update_url = extension_urls::GetWebstoreUpdateUrl();

  auto* supervised_user_extensions_delegate =
      ManagementAPI::GetFactoryInstance()
          ->Get(browser_context)
          ->GetSupervisedUserExtensionsDelegate();
  auto* extension_management_client =
      ExtensionsBrowserClient::Get()->GetExtensionManagementClient(
          browser_context);

  // Always use webstore update url to check the installation mode because this
  // function is used by webstore private API only and there may not be any
  // |Extension| instance. Note that we don't handle the case where an offstore
  // extension with an identical ID is installed.
  ManagedInstallationMode mode =
      extension_management_client->GetInstallationMode(extension_id,
                                                       update_url.spec());

  if (mode == ManagedInstallationMode::kForced ||
      mode == ManagedInstallationMode::kRecommended) {
    return kForceInstalled;
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);

  // Check if parent approval is needed for a supervised user to install
  // a new extension.
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  if (!registry->GetInstalledExtension(extension_id) &&
      supervised_user_extensions_delegate->IsChild() &&
      !supervised_user_extensions_delegate->CanSkipExtensionParentApprovals() &&
      !prefs->GetDict(prefs::kSupervisedUserApprovedExtensions)
           .contains(extension_id) &&
      manifest_type != Manifest::Type::kTheme) {
    return kCustodianApprovalRequiredForInstallation;
  }
  // Check if parent approval is needed for a supervised user to enable
  // an existing extension which is missing parental approval.
  if (registry->GetInstalledExtension(extension_id) &&
      ExtensionPrefs::Get(browser_context)
          ->HasDisableReason(
              extension_id,
              disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED)) {
    return kCustodianApprovalRequired;
  }

  if (registry->enabled_extensions().Contains(extension_id)) {
    return kEnabled;
  }

  if (registry->terminated_extensions().Contains(extension_id)) {
    return kTerminated;
  }

  if (registry->blocklisted_extensions().Contains(extension_id)) {
    return kBlocklisted;
  }

  bool is_blocked_by_policy = IsExtensionInstallBlockedByPolicy(
      extension_management_client, mode, extension_id, update_url.spec(),
      manifest_type, required_permission_set);

  // Check if it's possible for the extension to be requested.
  if (is_blocked_by_policy) {
    // The ability to request extension installs is not available if the
    // extension request policy is disabled.
    if (!prefs->GetBoolean(
            enterprise_reporting::kCloudExtensionRequestEnabled)) {
      return kBlockedByPolicy;
    }

    // An extension which is explicitly blocked by enterprise policy can't be
    // requested anymore.
    if (extension_management_client->IsInstallationExplicitlyBlocked(
            extension_id)) {
      return kBlockedByPolicy;
    }
  }

  // Check if the extension is using an unsupported manifest version.
  ManifestV2ExperimentManager* mv2_experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context);
  // At this point, we don't know what the extension manifest location really
  // is (because it's not installed). We pretend it will be kInternal, which
  // reflects an extension installed by the webstore.
  constexpr mojom::ManifestLocation kManifestLocation =
      mojom::ManifestLocation::kInternal;
  if (mv2_experiment_manager->ShouldBlockExtensionInstallation(
          extension_id, manifest_version, manifest_type, kManifestLocation,
          HashedExtensionId(extension_id))) {
    // The extension is using a deprecated manifest version and should not
    // be installable.
    return kDeprecatedManifestVersion;
  }

  // If an installed extension is disabled due to policy, return kCanRequest or
  // kRequestPending instead of kDisabled.
  // By doing so, user can still request an installed and policy blocked
  // extension.
  if (is_blocked_by_policy) {
    if (prefs->GetDict(enterprise_reporting::kCloudExtensionRequestIds)
            .Find(extension_id)) {
      return kRequestPending;
    }

    return kCanRequest;
  }

  if (registry->disabled_extensions().Contains(extension_id)) {
    bool is_corrupted =
        ExtensionPrefs::Get(browser_context)
            ->HasDisableReason(extension_id, disable_reason::DISABLE_CORRUPTED);
    return is_corrupted ? kCorrupted : kDisabled;
  }

  return kInstallable;
}

void OnCloudPolicyCheckDone(
    base::OnceCallback<void(ExtensionInstallStatus, std::u16string)> callback,
    bool can_install,
    std::u16string blocked_message) {
  std::move(callback).Run(can_install ? kInstallable : kBlockedByPolicy,
                          blocked_message);
}

std::u16string GetBlockedErrorMessage(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  CHECK(browser_context);
  auto* extension_management_client =
      ExtensionsBrowserClient::Get()->GetExtensionManagementClient(
          browser_context);

  std::u16string message_from_admin = base::UTF8ToUTF16(
      extension_management_client->BlockedInstallMessage(extension_id));
  if (!message_from_admin.empty()) {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_MESSAGE_FROM_ADMIN,
                                      message_from_admin);
  }
  return std::u16string();
}

}  // namespace

ExtensionInstallStatus GetWebstoreExtensionInstallStatus(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  // We don't know the extension's version, so we can't check
  // ExtensionInstallPolicyService. Only perform the other checks.
  return PerformSynchronousChecks(extension_id, browser_context,
                                  Manifest::Type::kUnknown, PermissionSet(),
                                  /*manifest_version=*/3);
}

void GetWebstoreExtensionInstallStatus(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context,
    const base::Version& extension_version,
    const Manifest::Type manifest_type,
    const PermissionSet& required_permission_set,
    int manifest_version,
    base::OnceCallback<void(ExtensionInstallStatus,
                            std::u16string blocked_message)> callback) {
  ExtensionInstallStatus status =
      PerformSynchronousChecks(extension_id, browser_context, manifest_type,
                               required_permission_set, manifest_version);

  if (status == kBlockedByPolicy) {
    std::move(callback).Run(
        status, GetBlockedErrorMessage(extension_id, browser_context));
    return;
  }

  if (status != kInstallable) {
    std::move(callback).Run(status, std::u16string());
    return;
  }

  if (extension_version.IsValid()) {
    ExtensionsBrowserClient::Get()->CanInstallExtensionByPolicy(
        browser_context, extension_id, extension_version,
        base::BindOnce(&OnCloudPolicyCheckDone, std::move(callback)));
    return;
  }

  std::move(callback).Run(kInstallable, std::u16string());
}

}  // namespace extensions
