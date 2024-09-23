// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/management/management_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/requirements_checker.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/api/management.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_handlers/replacement_apps.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::BrowserThread;
using extensions::mojom::ManifestLocation;

namespace keys = extension_management_api_constants;

namespace extensions {

namespace management = api::management;

namespace {

typedef std::vector<management::ExtensionInfo> ExtensionInfoList;
typedef std::vector<management::IconInfo> IconInfoList;

enum AutoConfirmForTest { DO_NOT_SKIP = 0, PROCEED, ABORT };

AutoConfirmForTest auto_confirm_for_test = DO_NOT_SKIP;

// Returns true if the extension should be exposed via the chrome.management
// API.
bool ShouldExposeViaManagementAPI(const Extension& extension) {
  return !Manifest::IsComponentLocation(extension.location());
}

std::vector<std::string> CreateWarningsList(const Extension* extension) {
  std::vector<std::string> warnings_list;
  for (const PermissionMessage& msg :
       extension->permissions_data()->GetPermissionMessages()) {
    warnings_list.push_back(base::UTF16ToUTF8(msg.message()));
  }

  return warnings_list;
}

std::vector<management::LaunchType> GetAvailableLaunchTypes(
    const Extension& extension,
    const ManagementAPIDelegate* delegate) {
  std::vector<management::LaunchType> launch_type_list;
  if (extension.is_platform_app()) {
    launch_type_list.push_back(management::LaunchType::kOpenAsWindow);
    return launch_type_list;
  }

  launch_type_list.push_back(management::LaunchType::kOpenAsRegularTab);
  launch_type_list.push_back(management::LaunchType::kOpenAsWindow);
  return launch_type_list;
}

management::ExtensionInfo CreateExtensionInfo(
    const Extension* source_extension,
    const Extension& extension,
    content::BrowserContext* context) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const ManagementAPIDelegate* delegate =
      ManagementAPI::GetFactoryInstance()->Get(context)->GetDelegate();
  management::ExtensionInfo info;

  info.id = extension.id();
  info.name = extension.name();
  info.short_name = extension.short_name();
  info.enabled = registry->enabled_extensions().Contains(info.id);
  info.offline_enabled = OfflineEnabledInfo::IsOfflineEnabled(&extension);
  info.version = extension.VersionString();
  if (!extension.version_name().empty()) {
    info.version_name = extension.version_name();
  }
  info.description = extension.description();
  info.options_url = OptionsPageInfo::GetOptionsPage(&extension).spec();
  info.homepage_url = ManifestURL::GetHomepageURL(&extension).spec();
  info.may_disable =
      !system->management_policy()->MustRemainEnabled(&extension, nullptr);
  info.is_app = extension.is_app();
  if (info.is_app) {
    if (extension.is_legacy_packaged_app()) {
      info.type = management::ExtensionType::kLegacyPackagedApp;
    } else if (extension.is_hosted_app()) {
      info.type = management::ExtensionType::kHostedApp;
    } else {
      info.type = management::ExtensionType::kPackagedApp;
    }
  } else if (extension.is_theme()) {
    info.type = management::ExtensionType::kTheme;
  } else if (extension.is_login_screen_extension()) {
    info.type = management::ExtensionType::kLoginScreenExtension;
  } else {
    info.type = management::ExtensionType::kExtension;
  }

  if (info.enabled) {
    info.disabled_reason = management::ExtensionDisabledReason::kNone;
  } else {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
    if (prefs->DidExtensionEscalatePermissions(extension.id())) {
      info.disabled_reason =
          management::ExtensionDisabledReason::kPermissionsIncrease;
    } else {
      info.disabled_reason = management::ExtensionDisabledReason::kUnknown;
    }

    info.may_enable = system->management_policy()->ExtensionMayModifySettings(
                          source_extension, &extension, nullptr) &&
                      !system->management_policy()->MustRemainDisabled(
                          &extension, nullptr, nullptr);
  }
  const GURL update_url = delegate->GetEffectiveUpdateURL(extension, context);
  if (!update_url.is_empty()) {
    info.update_url = update_url.spec();
  }

  if (extension.is_app()) {
    info.app_launch_url = delegate->GetFullLaunchURL(&extension).spec();
  }

  const ExtensionIconSet::IconMap& icons =
      IconsInfo::GetIcons(&extension).map();
  if (!icons.empty()) {
    info.icons.emplace();
    ExtensionIconSet::IconMap::const_iterator icon_iter;
    for (icon_iter = icons.begin(); icon_iter != icons.end(); ++icon_iter) {
      management::IconInfo icon_info;
      icon_info.size = icon_iter->first;
      GURL url = delegate->GetIconURL(&extension, icon_info.size,
                                      ExtensionIconSet::Match::kExactly, false);
      icon_info.url = url.spec();
      info.icons->push_back(std::move(icon_info));
    }
  }

  const std::set<std::string> perms =
      extension.permissions_data()->active_permissions().GetAPIsAsStrings();
  if (!perms.empty()) {
    std::set<std::string>::const_iterator perms_iter;
    for (perms_iter = perms.begin(); perms_iter != perms.end(); ++perms_iter) {
      info.permissions.push_back(*perms_iter);
    }
  }

  if (!extension.is_hosted_app()) {
    // Skip host permissions for hosted apps.
    const URLPatternSet& host_perms =
        extension.permissions_data()->active_permissions().explicit_hosts();
    if (!host_perms.is_empty()) {
      for (auto iter = host_perms.begin(); iter != host_perms.end(); ++iter) {
        info.host_permissions.push_back(iter->GetAsString());
      }
    }
  }

  switch (extension.location()) {
    case ManifestLocation::kInternal:
      info.install_type = management::ExtensionInstallType::kNormal;
      break;
    case ManifestLocation::kUnpacked:
    case ManifestLocation::kCommandLine:
      info.install_type = management::ExtensionInstallType::kDevelopment;
      break;
    case ManifestLocation::kExternalPref:
    case ManifestLocation::kExternalRegistry:
    case ManifestLocation::kExternalPrefDownload:
      info.install_type = management::ExtensionInstallType::kSideload;
      break;
    case ManifestLocation::kExternalPolicy:
    case ManifestLocation::kExternalPolicyDownload:
      info.install_type = management::ExtensionInstallType::kAdmin;
      break;
    case ManifestLocation::kInvalidLocation:
    case ManifestLocation::kComponent:
    case ManifestLocation::kExternalComponent:
      info.install_type = management::ExtensionInstallType::kOther;
      break;
  }

  info.launch_type = management::LaunchType::kNone;
  if (extension.is_app()) {
    LaunchType launch_type;
    if (extension.is_platform_app()) {
      launch_type = LAUNCH_TYPE_WINDOW;
    } else {
      launch_type =
          delegate->GetLaunchType(ExtensionPrefs::Get(context), &extension);
    }

    switch (launch_type) {
      case LAUNCH_TYPE_PINNED:
        info.launch_type = management::LaunchType::kOpenAsPinnedTab;
        break;
      case LAUNCH_TYPE_REGULAR:
        info.launch_type = management::LaunchType::kOpenAsRegularTab;
        break;
      case LAUNCH_TYPE_FULLSCREEN:
        info.launch_type = management::LaunchType::kOpenFullScreen;
        break;
      case LAUNCH_TYPE_WINDOW:
        info.launch_type = management::LaunchType::kOpenAsWindow;
        break;
      case LAUNCH_TYPE_INVALID:
      case NUM_LAUNCH_TYPES:
        NOTREACHED_IN_MIGRATION();
    }

    info.available_launch_types = GetAvailableLaunchTypes(extension, delegate);
  }

  return info;
}

void AddExtensionInfo(const Extension* source_extension,
                      const ExtensionSet& extensions,
                      ExtensionInfoList* extension_list,
                      content::BrowserContext* context) {
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    const Extension& extension = **iter;

    if (!ShouldExposeViaManagementAPI(extension)) {
      continue;
    }

    extension_list->push_back(
        CreateExtensionInfo(source_extension, extension, context));
  }
}

bool PlatformSupportsApprovalFlowForExtensions() {
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS devices have this feature already shipped.
  return true;
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(
      supervised_user::kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#else
  return false;
#endif
}
}  // namespace

ExtensionFunction::ResponseAction ManagementGetAllFunction::Run() {
  ExtensionInfoList extensions;
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  AddExtensionInfo(extension(), registry->enabled_extensions(), &extensions,
                   browser_context());
  AddExtensionInfo(extension(), registry->disabled_extensions(), &extensions,
                   browser_context());
  AddExtensionInfo(extension(), registry->terminated_extensions(), &extensions,
                   browser_context());

  return RespondNow(
      ArgumentList(management::GetAll::Results::Create(extensions)));
}

ExtensionFunction::ResponseAction ManagementGetFunction::Run() {
  std::optional<management::Get::Params> params =
      management::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  const Extension* target_extension =
      registry->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!target_extension) {
    return RespondNow(Error(keys::kNoExtensionError, params->id));
  }

  return RespondNow(ArgumentList(management::Get::Results::Create(
      CreateExtensionInfo(extension(), *target_extension, browser_context()))));
}

ExtensionFunction::ResponseAction ManagementGetSelfFunction::Run() {
  return RespondNow(ArgumentList(management::Get::Results::Create(
      CreateExtensionInfo(extension(), *extension_, browser_context()))));
}

ExtensionFunction::ResponseAction
ManagementGetPermissionWarningsByIdFunction::Run() {
  std::optional<management::GetPermissionWarningsById::Params> params =
      management::GetPermissionWarningsById::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return RespondNow(Error(keys::kNoExtensionError, params->id));
  }

  std::vector<std::string> warnings = CreateWarningsList(extension);
  return RespondNow(ArgumentList(
      management::GetPermissionWarningsById::Results::Create(warnings)));
}

ExtensionFunction::ResponseAction
ManagementGetPermissionWarningsByManifestFunction::Run() {
  std::optional<management::GetPermissionWarningsByManifest::Params> params =
      management::GetPermissionWarningsByManifest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  data_decoder::DataDecoder::ParseJsonIsolated(
      params->manifest_str,
      base::BindOnce(
          &ManagementGetPermissionWarningsByManifestFunction::OnParse, this));

  // Matched with a Release() in OnParse().
  AddRef();

  // Response is sent async in OnParse().
  return RespondLater();
}

void ManagementGetPermissionWarningsByManifestFunction::OnParse(
    data_decoder::DataDecoder::ValueOrError result) {
  Respond([&]() -> ResponseValue {
    ASSIGN_OR_RETURN(
        base::Value value, std::move(result),
        [&](std::string error) { return Error(std::move(error)); });

    const base::Value::Dict* parsed_manifest = value.GetIfDict();
    if (!parsed_manifest) {
      return Error(keys::kManifestParseError);
    }

    std::string error;
    scoped_refptr<Extension> extension =
        Extension::Create(base::FilePath(), ManifestLocation::kInvalidLocation,
                          *parsed_manifest, Extension::NO_FLAGS, &error);
    // TODO(lazyboy): Do we need to use |error|?
    if (!extension) {
      return Error(keys::kExtensionCreateError);
    }

    return ArgumentList(
        management::GetPermissionWarningsByManifest::Results::Create(
            CreateWarningsList(extension.get())));
  }());

  // Matched with AddRef() in Run().
  Release();
}

ExtensionFunction::ResponseAction ManagementLaunchAppFunction::Run() {
  std::optional<management::LaunchApp::Params> params =
      management::LaunchApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode()) {
    return RespondNow(Error(keys::kNotAllowedInKioskError));
  }

  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return RespondNow(Error(keys::kNoExtensionError, params->id));
  }
  if (!extension->is_app()) {
    return RespondNow(Error(keys::kNotAnAppError, params->id));
  }

  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  if (!delegate->LaunchAppFunctionDelegate(extension, browser_context())) {
    return RespondNow(Error(keys::kChromeAppsDeprecated, params->id));
  }
  return RespondNow(NoArguments());
}

ManagementSetEnabledFunction::ManagementSetEnabledFunction() = default;

ManagementSetEnabledFunction::~ManagementSetEnabledFunction() = default;

ExtensionFunction::ResponseAction ManagementSetEnabledFunction::Run() {
  std::optional<management::SetEnabled::Params> params =
      management::SetEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  extension_id_ = params->id;

  if (ExtensionsBrowserClient::Get()->IsAppModeForcedForApp(extension_id_)) {
    return RespondNow(Error(keys::kCannotChangePrimaryKioskAppError));
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  const Extension* target_extension = GetExtension();
  if (!target_extension || !ShouldExposeViaManagementAPI(*target_extension)) {
    return RespondNow(Error(keys::kNoExtensionError, extension_id_));
  }

  const ManagementPolicy* policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  if (!policy->ExtensionMayModifySettings(extension(), target_extension,
                                          /*error=*/nullptr)) {
    return RespondNow(Error(keys::kUserCantModifyError, extension_id_));
  }

  // Do nothing if method wants to enable an already enabled extension, and
  // vice-versa.
  bool should_enable = params->enabled;
  bool currently_enabled =
      registry->enabled_extensions().Contains(extension_id_) ||
      registry->terminated_extensions().Contains(extension_id_);
  if ((should_enable && currently_enabled) ||
      (!should_enable && !currently_enabled)) {
    return RespondNow(NoArguments());
  }

  if (PlatformSupportsApprovalFlowForExtensions() &&
      IsSupervisedExtensionApprovalFlowRequired(target_extension)) {
    // Either ask for parent permission or notify the child that their parent
    // has disabled this action.
    auto approval_callback = base::BindOnce(
        &ManagementSetEnabledFunction::OnSupervisedExtensionApprovalDone, this);
    AddRef();  // Matched in OnSupervisedExtensionApprovalDone().

    SupervisedUserExtensionsDelegate* supervised_user_extensions_delegate =
        ManagementAPI::GetFactoryInstance()
            ->Get(browser_context())
            ->GetSupervisedUserExtensionsDelegate();
    CHECK(supervised_user_extensions_delegate)
        << "Implied by IsSupervisedExtensionApprovalFlowRequired";
    supervised_user_extensions_delegate->RequestToEnableExtensionOrShowError(
        *target_extension, GetSenderWebContents(),
        SupervisedUserExtensionParentApprovalEntryPoint::
            kOnExtensionManagementSetEnabledOperation,
        std::move(approval_callback));
    return RespondLater();
  }

  // Disable extension.
  if (!should_enable) {
    const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                                ->Get(browser_context())
                                                ->GetDelegate();
    delegate->DisableExtension(
        browser_context(), extension(), extension_id_,
        Manifest::IsPolicyLocation(target_extension->location())
            ? disable_reason::DISABLE_BLOCKED_BY_POLICY
            : disable_reason::DISABLE_USER_ACTION);
    return RespondNow(NoArguments());
  }

  // Enable extension.

  // Cannot enable policy disabled extensions.
  if (policy->MustRemainDisabled(target_extension, /*reason=*/nullptr,
                                 /*error=*/nullptr)) {
    return RespondNow(Error(keys::kUserCantModifyError, extension_id_));
  }

  // Start the various checks needed before enabling an extension.
  AddRef();  // Balanced in FinishEnable().
  CheckRequirements(*target_extension);

  // The function may have already responded, if CheckRequirements()
  // responded synchronously.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void ManagementSetEnabledFunction::CheckRequirements(
    const Extension& extension) {
  if (HasUnsupportedRequirements(extension_id_)) {
    // Recheck the requirements.
    requirements_checker_ = std::make_unique<RequirementsChecker>(&extension);
    requirements_checker_->Start(base::BindOnce(
        &ManagementSetEnabledFunction::OnRequirementsChecked, this));
    return;
  }

  // Call OnRequirementsChecked with empty errors, since requirements are
  // already supported.
  PreloadCheck::Errors errors;
  OnRequirementsChecked(errors);
}

void ManagementSetEnabledFunction::OnRequirementsChecked(
    const PreloadCheck::Errors& errors) {
  if (!errors.empty()) {
    // TODO(devlin): Should we really be noisy here all the time?
    FinishEnable(
        Error(keys::kMissingRequirementsError,
              base::UTF16ToUTF8(requirements_checker_->GetErrorMessage())));
    return;
  }

  // Continue with the enable extension flow.
  CheckPermissionsIncrease();
}

void ManagementSetEnabledFunction::CheckPermissionsIncrease() {
  // Extension could have been uninstalled externally while previous check was
  // happening.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    FinishEnable(Error(keys::kNoExtensionError));
    return;
  }

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  if (prefs->DidExtensionEscalatePermissions(extension_id_)) {
    if (!user_gesture()) {
      FinishEnable(Error(keys::kGestureNeededForEscalationError));
      return;
    }

    const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                                ->Get(browser_context())
                                                ->GetDelegate();
    install_prompt_ = delegate->SetEnabledFunctionDelegate(
        GetSenderWebContents(), browser_context(), extension,
        base::BindOnce(
            &ManagementSetEnabledFunction::OnPermissionsIncreaseChecked, this));
    return;
  }

  // Call OnPermissionsIncreaseChecked with permissions allowed set to true,
  // since there was no permissions increase.
  OnPermissionsIncreaseChecked(/*permissions_allowed=*/true);
}

void ManagementSetEnabledFunction::OnPermissionsIncreaseChecked(
    bool permissions_allowed) {
  if (!permissions_allowed) {
    FinishEnable(Error(keys::kUserDidNotReEnableError));
    return;
  }

  // Continue with the enable extension flow.
  CheckManifestV2Deprecation();
}

void ManagementSetEnabledFunction::CheckManifestV2Deprecation() {
  // Extension can be uninstalled externally while the previous check was
  // happening async.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    FinishEnable(Error(keys::kNoExtensionError));
    return;
  }

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  if (prefs->HasDisableReason(
          extension_id_,
          disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION)) {
    if (!user_gesture()) {
      FinishEnable(Error(keys::kGestureNeededForMV2DeprecationReEnableError));
      return;
    }

    // Show re-enable dialog.
    const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                                ->Get(browser_context())
                                                ->GetDelegate();
    delegate->ShowMv2DeprecationReEnableDialog(
        browser_context(), GetSenderWebContents(), *extension,
        base::BindOnce(
            &ManagementSetEnabledFunction::OnManifestV2DeprecationChecked,
            this));
    return;
  }

  // Call OnManifestV2DeprecationChecked with enable allowed set to true,
  // since the MV2 deprecation doesn't affect this extension.
  OnManifestV2DeprecationChecked(/*enable_allowed=*/true);
}

void ManagementSetEnabledFunction::OnManifestV2DeprecationChecked(
    bool enable_allowed) {
  if (!enable_allowed) {
    FinishEnable(Error(keys::kUserDidNotReEnableError));
    return;
  }

  // Extension could have been uninstalled externally while previous check was
  // happening.
  const Extension* extension = GetExtension();
  if (!extension) {
    FinishEnable(Error(keys::kNoExtensionError));
    return;
  }

  // This was the last check in the enable flow. We can now finish enabling
  // the extension.
  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  delegate->EnableExtension(browser_context(), extension_id_);
  FinishEnable(NoArguments());
}

void ManagementSetEnabledFunction::FinishEnable(ResponseValue response_value) {
  Respond(std::move(response_value));
  Release();  // Balanced in Run().
}

bool ManagementSetEnabledFunction::HasUnsupportedRequirements(
    const ExtensionId& extension_id) const {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  return prefs->HasDisableReason(
      extension_id, disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT);
}

bool ManagementSetEnabledFunction::IsSupervisedExtensionApprovalFlowRequired(
    const Extension* target_extension) const {
  SupervisedUserExtensionsDelegate* supervised_user_extensions_delegate =
      ManagementAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->GetSupervisedUserExtensionsDelegate();

  if (!supervised_user_extensions_delegate) {
    return false;
  }
  if (!supervised_user_extensions_delegate->IsChild()) {
    return false;
  }
  // Don't prompt the user if the extension has unsupported requirements.
  // TODO(crbug.com/40127008): If OnRequirementsChecked() passes, the extension
  // will enable, bypassing parent approval.
  if (HasUnsupportedRequirements(extension_id_)) {
    return false;
  }
  // Only ask for parent approval if the extension still requires
  // approval.
  if (supervised_user_extensions_delegate->IsExtensionAllowedByParent(
          *target_extension)) {
    return false;
  }
  return true;
}

void ManagementSetEnabledFunction::OnSupervisedExtensionApprovalDone(
    SupervisedUserExtensionsDelegate::ExtensionApprovalResult result) {
  // TODO(crbug.com/1320442): Investigate whether ENABLE_SUPERVISED_USERS can
  // be ported to //extensions.
  switch (result) {
    case SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kApproved: {
      // Grant parent approval.
      extensions::SupervisedUserExtensionsDelegate*
          supervised_user_extensions_delegate =
              extensions::ManagementAPI::GetFactoryInstance()
                  ->Get(browser_context())
                  ->GetSupervisedUserExtensionsDelegate();
      CHECK(supervised_user_extensions_delegate);
      auto* registry = ExtensionRegistry::Get(browser_context());
      const Extension* extension =
          registry->GetInstalledExtension(extension_id_);
      CHECK(extension);
      supervised_user_extensions_delegate->AddExtensionApproval(*extension);

      const ManagementAPIDelegate* delegate =
          ManagementAPI::GetFactoryInstance()
              ->Get(browser_context())
              ->GetDelegate();
      delegate->EnableExtension(browser_context(), extension_id_);
      Respond(NoArguments());
      break;
    }

    case SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kCanceled: {
      Respond(Error(keys::kUserDidNotReEnableError));
      break;
    }

    case SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kFailed: {
      Respond(Error(keys::kParentPermissionFailedError));
      break;
    }

    case SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kBlocked: {
      Respond(Error(keys::kUserCantModifyError, extension_id_));
      break;
    }
  }
  // Matches the AddRef in Run().
  Release();
}

const Extension* ManagementSetEnabledFunction::GetExtension() {
  return ExtensionRegistry::Get(browser_context())
      ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
}

ManagementUninstallFunctionBase::ManagementUninstallFunctionBase() = default;

ManagementUninstallFunctionBase::~ManagementUninstallFunctionBase() = default;

bool ManagementUninstallFunctionBase::ShouldKeepWorkerAliveIndefinitely() {
  // `management.uninstall()` can display and block on an uninstall dialog while
  // waiting for user confirmation.
  return true;
}

ExtensionFunction::ResponseAction ManagementUninstallFunctionBase::Uninstall(
    const std::string& target_extension_id,
    bool show_confirm_dialog) {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode()) {
    return RespondNow(Error(keys::kNotAllowedInKioskError));
  }

  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  target_extension_id_ = target_extension_id;
  const Extension* target_extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(target_extension_id_,
                             ExtensionRegistry::EVERYTHING);
  if (!target_extension || !ShouldExposeViaManagementAPI(*target_extension)) {
    return RespondNow(Error(keys::kNoExtensionError, target_extension_id_));
  }

  ManagementPolicy* policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  if (!policy->UserMayModifySettings(target_extension, nullptr) ||
      policy->MustRemainInstalled(target_extension, nullptr)) {
    return RespondNow(Error(keys::kUserCantModifyError, target_extension_id_));
  }

  // A null extension() should only happen if the call is coming from WebUI or
  // the new Webstore which is a webpage the management API is exposed on.
  DCHECK(extension() || source_context_type() == mojom::ContextType::kWebUi ||
         extension_urls::IsWebstoreDomain(source_url()));

  bool self_uninstall = extension() && extension_id() == target_extension_id_;
  // We need to show a dialog for any extension uninstalling another extension.
  show_confirm_dialog |= !self_uninstall;

  if (show_confirm_dialog && !user_gesture()) {
    return RespondNow(Error(keys::kGestureNeededForUninstallError));
  }

  if (show_confirm_dialog) {
    // We show the programmatic uninstall ui for extensions uninstalling
    // other extensions.
    bool show_programmatic_uninstall_ui =
        !self_uninstall && extension() &&
        extension()->id() != extensions::kWebStoreAppId;
    AddRef();  // Balanced in OnExtensionUninstallDialogClosed.
    // TODO(devlin): A method called "UninstallFunctionDelegate" does not in
    // any way imply that this actually creates a dialog and runs it.
    uninstall_dialog_ = delegate->UninstallFunctionDelegate(
        this, target_extension, show_programmatic_uninstall_ui);
  } else {  // No confirm dialog.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ManagementUninstallFunctionBase::UninstallExtension,
                       this));
  }

  return RespondLater();
}

void ManagementUninstallFunctionBase::Finish(bool did_start_uninstall,
                                             const std::string& error) {
  Respond(did_start_uninstall ? NoArguments() : Error(error));
}

void ManagementUninstallFunctionBase::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const std::u16string& error) {
  Finish(did_start_uninstall,
         ErrorUtils::FormatErrorMessage(keys::kUninstallCanceledError,
                                        target_extension_id_));
  Release();  // Balanced in Uninstall().
}

void ManagementUninstallFunctionBase::UninstallExtension() {
  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* target_extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(target_extension_id_,
                             ExtensionRegistry::EVERYTHING);
  std::string error;
  bool success = false;
  if (target_extension) {
    const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                                ->Get(browser_context())
                                                ->GetDelegate();
    std::u16string utf16_error;
    success = delegate->UninstallExtension(
        browser_context(), target_extension_id_,
        extensions::UNINSTALL_REASON_MANAGEMENT_API, &utf16_error);
    error = base::UTF16ToUTF8(utf16_error);
  } else {
    error = ErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                           target_extension_id_);
  }
  Finish(success, error);
}

ManagementUninstallFunction::ManagementUninstallFunction() {}

ManagementUninstallFunction::~ManagementUninstallFunction() {}

ExtensionFunction::ResponseAction ManagementUninstallFunction::Run() {
  std::optional<management::Uninstall::Params> params =
      management::Uninstall::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool show_confirm_dialog =
      params->options && params->options->show_confirm_dialog.value_or(false);
  return Uninstall(params->id, show_confirm_dialog);
}

ManagementUninstallSelfFunction::ManagementUninstallSelfFunction() {}

ManagementUninstallSelfFunction::~ManagementUninstallSelfFunction() {}

ExtensionFunction::ResponseAction ManagementUninstallSelfFunction::Run() {
  std::optional<management::UninstallSelf::Params> params =
      management::UninstallSelf::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(extension_.get());

  bool show_confirm_dialog =
      params->options && params->options->show_confirm_dialog.value_or(false);
  return Uninstall(extension_->id(), show_confirm_dialog);
}

ManagementCreateAppShortcutFunction::ManagementCreateAppShortcutFunction() {}

ManagementCreateAppShortcutFunction::~ManagementCreateAppShortcutFunction() {}

// static
void ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(
    bool should_proceed) {
  auto_confirm_for_test = should_proceed ? PROCEED : ABORT;
}

void ManagementCreateAppShortcutFunction::OnCloseShortcutPrompt(bool created) {
  Respond(created ? NoArguments() : Error(keys::kCreateShortcutCanceledError));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction ManagementCreateAppShortcutFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode()) {
    return RespondNow(Error(keys::kNotAllowedInKioskError));
  }

  if (!user_gesture()) {
    return RespondNow(Error(keys::kGestureNeededForCreateAppShortcutError));
  }

  std::optional<management::CreateAppShortcut::Params> params =
      management::CreateAppShortcut::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return RespondNow(Error(
        ErrorUtils::FormatErrorMessage(keys::kNoExtensionError, params->id)));
  }

  if (!extension->is_app()) {
    return RespondNow(Error(
        ErrorUtils::FormatErrorMessage(keys::kNotAnAppError, params->id)));
  }

#if BUILDFLAG(IS_MAC)
  if (!extension->is_platform_app()) {
    return RespondNow(Error(keys::kCreateOnlyPackagedAppShortcutMac));
  }
#endif

  if (auto_confirm_for_test != DO_NOT_SKIP) {
    // Matched with a Release() in OnCloseShortcutPrompt().
    AddRef();

    OnCloseShortcutPrompt(auto_confirm_for_test == PROCEED);
    // OnCloseShortcutPrompt() might have called Respond() already.
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  std::string error;
  if (ManagementAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->GetDelegate()
          ->CreateAppShortcutFunctionDelegate(this, extension, &error)) {
    // Matched with a Release() in OnCloseShortcutPrompt().
    AddRef();
    // Response is sent async in OnCloseShortcutPrompt().
    return RespondLater();
  } else {
    return RespondNow(Error(std::move(error)));
  }
}

ExtensionFunction::ResponseAction ManagementSetLaunchTypeFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode()) {
    return RespondNow(Error(keys::kNotAllowedInKioskError));
  }

  if (!user_gesture()) {
    return RespondNow(Error(keys::kGestureNeededForSetLaunchTypeError));
  }

  std::optional<management::SetLaunchType::Params> params =
      management::SetLaunchType::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  if (!extension) {
    return RespondNow(Error(keys::kNoExtensionError, params->id));
  }

  if (!extension->is_app()) {
    return RespondNow(Error(keys::kNotAnAppError, params->id));
  }

  std::vector<management::LaunchType> available_launch_types =
      GetAvailableLaunchTypes(*extension, delegate);

  management::LaunchType app_launch_type = params->launch_type;
  if (!base::Contains(available_launch_types, app_launch_type)) {
    return RespondNow(Error(keys::kLaunchTypeNotAvailableError));
  }

  LaunchType launch_type = LAUNCH_TYPE_DEFAULT;
  switch (app_launch_type) {
    case management::LaunchType::kOpenAsPinnedTab:
      launch_type = LAUNCH_TYPE_PINNED;
      break;
    case management::LaunchType::kOpenAsRegularTab:
      launch_type = LAUNCH_TYPE_REGULAR;
      break;
    case management::LaunchType::kOpenFullScreen:
      launch_type = LAUNCH_TYPE_FULLSCREEN;
      break;
    case management::LaunchType::kOpenAsWindow:
      launch_type = LAUNCH_TYPE_WINDOW;
      break;
    case management::LaunchType::kNone:
      NOTREACHED_IN_MIGRATION();
  }

  delegate->SetLaunchType(browser_context(), params->id, launch_type);

  return RespondNow(NoArguments());
}

ManagementGenerateAppForLinkFunction::ManagementGenerateAppForLinkFunction() =
    default;

ManagementGenerateAppForLinkFunction::~ManagementGenerateAppForLinkFunction() =
    default;

void ManagementGenerateAppForLinkFunction::FinishCreateWebApp(
    const std::string& web_app_id,
    bool install_success) {
  if (install_success) {
    Respond(ArgumentList(management::GenerateAppForLink::Results::Create(
        app_for_link_delegate_->CreateExtensionInfoFromWebApp(
            web_app_id, browser_context()))));
  } else {
    Respond(Error(keys::kGenerateAppForLinkInstallError));
  }
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction ManagementGenerateAppForLinkFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode()) {
    return RespondNow(Error(keys::kNotAllowedInKioskError));
  }

  if (!user_gesture()) {
    return RespondNow(Error(keys::kGestureNeededForGenerateAppForLinkError));
  }

  std::optional<management::GenerateAppForLink::Params> params =
      management::GenerateAppForLink::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL launch_url(params->url);
  if (!launch_url.is_valid() || !launch_url.SchemeIsHTTPOrHTTPS()) {
    return RespondNow(Error(
        ErrorUtils::FormatErrorMessage(keys::kInvalidURLError, params->url)));
  }

  if (params->title.empty()) {
    return RespondNow(Error(keys::kEmptyTitleError));
  }

  app_for_link_delegate_ =
      ManagementAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->GetDelegate()
          ->GenerateAppForLinkFunctionDelegate(this, browser_context(),
                                               params->title, launch_url);

  // Matched with a Release() in FinishCreateWebApp().
  AddRef();

  // Response is sent async in FinishCreateWebApp().
  return RespondLater();
}

ManagementInstallReplacementWebAppFunction::
    ManagementInstallReplacementWebAppFunction() {}

ManagementInstallReplacementWebAppFunction::
    ~ManagementInstallReplacementWebAppFunction() {}

ExtensionFunction::ResponseAction
ManagementInstallReplacementWebAppFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode()) {
    return RespondNow(Error(keys::kNotAllowedInKioskError));
  }

  if (!extension()->from_webstore()) {
    return RespondNow(
        Error(keys::kInstallReplacementWebAppNotFromWebstoreError));
  }

  if (!user_gesture()) {
    return RespondNow(
        Error(keys::kGestureNeededForInstallReplacementWebAppError));
  }

  DCHECK(ReplacementAppsInfo::HasReplacementWebApp(extension()));
  const GURL& web_app_url =
      ReplacementAppsInfo::GetReplacementWebApp(extension());

  DCHECK(web_app_url.is_valid());
  DCHECK(web_app_url.SchemeIs(url::kHttpsScheme));

  auto* api_delegate = ManagementAPI::GetFactoryInstance()
                           ->Get(browser_context())
                           ->GetDelegate();
  if (!api_delegate->CanContextInstallWebApps(browser_context())) {
    return RespondNow(
        Error(keys::kInstallReplacementWebAppInvalidContextError));
  }

  // Adds a ref-count.
  api_delegate->InstallOrLaunchReplacementWebApp(
      browser_context(), web_app_url,
      base::BindOnce(
          &ManagementInstallReplacementWebAppFunction::FinishResponse, this));

  // Response is sent async in FinishResponse().
  return RespondLater();
}

void ManagementInstallReplacementWebAppFunction::FinishResponse(
    ManagementAPIDelegate::InstallOrLaunchWebAppResult result) {
  switch (result) {
    case ManagementAPIDelegate::InstallOrLaunchWebAppResult::kSuccess:
      Respond(NoArguments());
      break;
    case ManagementAPIDelegate::InstallOrLaunchWebAppResult::kInvalidWebApp:
      Respond(Error(keys::kInstallReplacementWebAppInvalidWebAppError));
      break;
    case ManagementAPIDelegate::InstallOrLaunchWebAppResult::kUnknownError:
      Respond(Error(keys::kGenerateAppForLinkInstallError));
      break;
  }
}

ManagementEventRouter::ManagementEventRouter(content::BrowserContext* context)
    : browser_context_(context) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
}

ManagementEventRouter::~ManagementEventRouter() = default;

void ManagementEventRouter::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  BroadcastEvent(extension, events::MANAGEMENT_ON_ENABLED,
                 management::OnEnabled::kEventName);
}

void ManagementEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  BroadcastEvent(extension, events::MANAGEMENT_ON_DISABLED,
                 management::OnDisabled::kEventName);
}

void ManagementEventRouter::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  BroadcastEvent(extension, events::MANAGEMENT_ON_INSTALLED,
                 management::OnInstalled::kEventName);
}

void ManagementEventRouter::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  BroadcastEvent(extension, events::MANAGEMENT_ON_UNINSTALLED,
                 management::OnUninstalled::kEventName);
}

void ManagementEventRouter::BroadcastEvent(
    const Extension* extension,
    events::HistogramValue histogram_value,
    const char* event_name) {
  if (!ShouldExposeViaManagementAPI(*extension)) {
    return;
  }
  base::Value::List args;
  if (event_name == management::OnUninstalled::kEventName) {
    args.Append(extension->id());
  } else {
    args.Append(
        CreateExtensionInfo(nullptr, *extension, browser_context_).ToValue());
  }

  EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::make_unique<Event>(histogram_value, event_name,
                                               std::move(args)));
}

ManagementAPI::ManagementAPI(content::BrowserContext* context)
    : browser_context_(context),
      delegate_(ExtensionsAPIClient::Get()->CreateManagementAPIDelegate()),
      supervised_user_extensions_delegate_(
          ExtensionsAPIClient::Get()->CreateSupervisedUserExtensionsDelegate(
              browser_context_)) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, management::OnInstalled::kEventName);
  event_router->RegisterObserver(this, management::OnUninstalled::kEventName);
  event_router->RegisterObserver(this, management::OnEnabled::kEventName);
  event_router->RegisterObserver(this, management::OnDisabled::kEventName);
}

ManagementAPI::~ManagementAPI() {}

void ManagementAPI::Shutdown() {
  // Ensure that SupervisedUserExtensionsDelegate is released prior to
  // destruction to release the raw pointer to browser_context_.
  supervised_user_extensions_delegate_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ManagementAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ManagementAPI>*
ManagementAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ManagementAPI::OnListenerAdded(const EventListenerInfo& details) {
  management_event_router_ =
      std::make_unique<ManagementEventRouter>(browser_context_);
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
