// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/management/management_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/requirements_checker.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/api/management.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_handlers/replacement_apps.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::BrowserThread;

namespace keys = extension_management_api_constants;

namespace extensions {

namespace management = api::management;

namespace {

typedef std::vector<management::ExtensionInfo> ExtensionInfoList;
typedef std::vector<management::IconInfo> IconInfoList;

enum AutoConfirmForTest { DO_NOT_SKIP = 0, PROCEED, ABORT };

AutoConfirmForTest auto_confirm_for_test = DO_NOT_SKIP;

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
    launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_WINDOW);
    return launch_type_list;
  }

  launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB);
  launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_WINDOW);
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
  if (!extension.version_name().empty())
    info.version_name.reset(new std::string(extension.version_name()));
  info.description = extension.description();
  info.options_url = OptionsPageInfo::GetOptionsPage(&extension).spec();
  info.homepage_url.reset(
      new std::string(ManifestURL::GetHomepageURL(&extension).spec()));
  info.may_disable =
      !system->management_policy()->MustRemainEnabled(&extension, nullptr);
  info.is_app = extension.is_app();
  if (info.is_app) {
    if (extension.is_legacy_packaged_app())
      info.type = management::EXTENSION_TYPE_LEGACY_PACKAGED_APP;
    else if (extension.is_hosted_app())
      info.type = management::EXTENSION_TYPE_HOSTED_APP;
    else
      info.type = management::EXTENSION_TYPE_PACKAGED_APP;
  } else if (extension.is_theme()) {
    info.type = management::EXTENSION_TYPE_THEME;
  } else if (extension.is_login_screen_extension()) {
    info.type = management::EXTENSION_TYPE_LOGIN_SCREEN_EXTENSION;
  } else {
    info.type = management::EXTENSION_TYPE_EXTENSION;
  }

  if (info.enabled) {
    info.disabled_reason = management::EXTENSION_DISABLED_REASON_NONE;
  } else {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
    if (prefs->DidExtensionEscalatePermissions(extension.id())) {
      info.disabled_reason =
          management::EXTENSION_DISABLED_REASON_PERMISSIONS_INCREASE;
    } else {
      info.disabled_reason = management::EXTENSION_DISABLED_REASON_UNKNOWN;
    }

    info.may_enable = std::make_unique<bool>(
        system->management_policy()->ExtensionMayModifySettings(
            source_extension, &extension, nullptr) &&
        !system->management_policy()->MustRemainDisabled(&extension, nullptr,
                                                         nullptr));
  }

  if (!ManifestURL::GetUpdateURL(&extension).is_empty()) {
    info.update_url.reset(
        new std::string(ManifestURL::GetUpdateURL(&extension).spec()));
  }

  if (extension.is_app()) {
    info.app_launch_url.reset(
        new std::string(delegate->GetFullLaunchURL(&extension).spec()));
  }

  const ExtensionIconSet::IconMap& icons =
      IconsInfo::GetIcons(&extension).map();
  if (!icons.empty()) {
    info.icons.reset(new IconInfoList());
    ExtensionIconSet::IconMap::const_iterator icon_iter;
    for (icon_iter = icons.begin(); icon_iter != icons.end(); ++icon_iter) {
      management::IconInfo icon_info;
      icon_info.size = icon_iter->first;
      GURL url = delegate->GetIconURL(&extension, icon_info.size,
                                      ExtensionIconSet::MATCH_EXACTLY, false);
      icon_info.url = url.spec();
      info.icons->push_back(std::move(icon_info));
    }
  }

  const std::set<std::string> perms =
      extension.permissions_data()->active_permissions().GetAPIsAsStrings();
  if (!perms.empty()) {
    std::set<std::string>::const_iterator perms_iter;
    for (perms_iter = perms.begin(); perms_iter != perms.end(); ++perms_iter)
      info.permissions.push_back(*perms_iter);
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
    case Manifest::INTERNAL:
      info.install_type = management::EXTENSION_INSTALL_TYPE_NORMAL;
      break;
    case Manifest::UNPACKED:
    case Manifest::COMMAND_LINE:
      info.install_type = management::EXTENSION_INSTALL_TYPE_DEVELOPMENT;
      break;
    case Manifest::EXTERNAL_PREF:
    case Manifest::EXTERNAL_REGISTRY:
    case Manifest::EXTERNAL_PREF_DOWNLOAD:
      info.install_type = management::EXTENSION_INSTALL_TYPE_SIDELOAD;
      break;
    case Manifest::EXTERNAL_POLICY:
    case Manifest::EXTERNAL_POLICY_DOWNLOAD:
      info.install_type = management::EXTENSION_INSTALL_TYPE_ADMIN;
      break;
    case Manifest::NUM_LOCATIONS:
      NOTREACHED();
      FALLTHROUGH;
    case Manifest::INVALID_LOCATION:
    case Manifest::COMPONENT:
    case Manifest::EXTERNAL_COMPONENT:
      info.install_type = management::EXTENSION_INSTALL_TYPE_OTHER;
      break;
  }

  info.launch_type = management::LAUNCH_TYPE_NONE;
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
        info.launch_type = management::LAUNCH_TYPE_OPEN_AS_PINNED_TAB;
        break;
      case LAUNCH_TYPE_REGULAR:
        info.launch_type = management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB;
        break;
      case LAUNCH_TYPE_FULLSCREEN:
        info.launch_type = management::LAUNCH_TYPE_OPEN_FULL_SCREEN;
        break;
      case LAUNCH_TYPE_WINDOW:
        info.launch_type = management::LAUNCH_TYPE_OPEN_AS_WINDOW;
        break;
      case LAUNCH_TYPE_INVALID:
      case NUM_LAUNCH_TYPES:
        NOTREACHED();
    }

    info.available_launch_types.reset(new std::vector<management::LaunchType>(
        GetAvailableLaunchTypes(extension, delegate)));
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

    if (!extension.ShouldExposeViaManagementAPI())
      continue;

    extension_list->push_back(
        CreateExtensionInfo(source_extension, extension, context));
  }
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
  std::unique_ptr<management::Get::Params> params(
      management::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  const Extension* target_extension =
      registry->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!target_extension)
    return RespondNow(Error(keys::kNoExtensionError, params->id));

  return RespondNow(ArgumentList(management::Get::Results::Create(
      CreateExtensionInfo(extension(), *target_extension, browser_context()))));
}

ExtensionFunction::ResponseAction ManagementGetSelfFunction::Run() {
  return RespondNow(ArgumentList(management::Get::Results::Create(
      CreateExtensionInfo(extension(), *extension_, browser_context()))));
}

ExtensionFunction::ResponseAction
ManagementGetPermissionWarningsByIdFunction::Run() {
  std::unique_ptr<management::GetPermissionWarningsById::Params> params(
      management::GetPermissionWarningsById::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension)
    return RespondNow(Error(keys::kNoExtensionError, params->id));

  std::vector<std::string> warnings = CreateWarningsList(extension);
  return RespondNow(ArgumentList(
      management::GetPermissionWarningsById::Results::Create(warnings)));
}

ExtensionFunction::ResponseAction
ManagementGetPermissionWarningsByManifestFunction::Run() {
  std::unique_ptr<management::GetPermissionWarningsByManifest::Params> params(
      management::GetPermissionWarningsByManifest::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();

  if (delegate) {
    delegate->GetPermissionWarningsByManifestFunctionDelegate(
        this, params->manifest_str);

    // Matched with a Release() in OnParse().
    AddRef();

    // Response is sent async in OnParse().
    return RespondLater();
  } else {
    // TODO(lfg) add error string
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }
}
void ManagementGetPermissionWarningsByManifestFunction::OnParse(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    Respond(Error(*result.error));

    // Matched with AddRef() in Run().
    Release();
    return;
  }

  const base::DictionaryValue* parsed_manifest;
  if (!result.value->GetAsDictionary(&parsed_manifest)) {
    Respond(Error(keys::kManifestParseError));
    Release();
    return;
  }

  std::string error;
  scoped_refptr<Extension> extension =
      Extension::Create(base::FilePath(), Manifest::INVALID_LOCATION,
                        *parsed_manifest, Extension::NO_FLAGS, &error);
  // TODO(lazyboy): Do we need to use |error|?
  if (!extension) {
    Respond(Error(keys::kExtensionCreateError));
    Release();
    return;
  }

  std::vector<std::string> warnings = CreateWarningsList(extension.get());
  Respond(ArgumentList(
      management::GetPermissionWarningsByManifest::Results::Create(warnings)));

  // Matched with AddRef() in Run().
  Release();
}

ExtensionFunction::ResponseAction ManagementLaunchAppFunction::Run() {
  std::unique_ptr<management::LaunchApp::Params> params(
      management::LaunchApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    return RespondNow(Error(keys::kNotAllowedInKioskError));

  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension)
    return RespondNow(Error(keys::kNoExtensionError, params->id));
  if (!extension->is_app())
    return RespondNow(Error(keys::kNotAnAppError, params->id));

  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  delegate->LaunchAppFunctionDelegate(extension, browser_context());
  return RespondNow(NoArguments());
}

ManagementSetEnabledFunction::ManagementSetEnabledFunction() {
}

ManagementSetEnabledFunction::~ManagementSetEnabledFunction() {
}

ExtensionFunction::ResponseAction ManagementSetEnabledFunction::Run() {
  std::unique_ptr<management::SetEnabled::Params> params(
      management::SetEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  extension_id_ = params->id;

  if (ExtensionsBrowserClient::Get()->IsAppModeForcedForApp(extension_id_))
    return RespondNow(Error(keys::kCannotChangePrimaryKioskAppError));

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();

  const Extension* target_extension =
      registry->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (!target_extension || !target_extension->ShouldExposeViaManagementAPI())
    return RespondNow(Error(keys::kNoExtensionError, extension_id_));

  bool enabled = params->enabled;
  const ManagementPolicy* policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  if (!policy->ExtensionMayModifySettings(extension(), target_extension,
                                          nullptr) ||
      (enabled &&
       policy->MustRemainDisabled(target_extension, nullptr, nullptr))) {
    return RespondNow(Error(keys::kUserCantModifyError, extension_id_));
  }

  bool currently_enabled =
      registry->enabled_extensions().Contains(extension_id_) ||
      registry->terminated_extensions().Contains(extension_id_);

  if (!currently_enabled && enabled) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
    if (prefs->DidExtensionEscalatePermissions(extension_id_)) {
      if (!user_gesture())
        return RespondNow(Error(keys::kGestureNeededForEscalationError));

      AddRef();  // Matched in OnInstallPromptDone().
      install_prompt_ = delegate->SetEnabledFunctionDelegate(
          GetSenderWebContents(), browser_context(), target_extension,
          base::Bind(&ManagementSetEnabledFunction::OnInstallPromptDone, this));
      return RespondLater();
    }
    if (prefs->GetDisableReasons(extension_id_) &
        disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT) {
      // Recheck the requirements.
      requirements_checker_ =
          std::make_unique<RequirementsChecker>(target_extension);
      requirements_checker_->Start(
          base::Bind(&ManagementSetEnabledFunction::OnRequirementsChecked,
                     this));  // This bind creates a reference.
      return RespondLater();
    }
    delegate->EnableExtension(browser_context(), extension_id_);
  } else if (currently_enabled && !params->enabled) {
    delegate->DisableExtension(
        browser_context(), extension(), extension_id_,
        Manifest::IsPolicyLocation(target_extension->location())
            ? disable_reason::DISABLE_BLOCKED_BY_POLICY
            : disable_reason::DISABLE_USER_ACTION);
  }

  return RespondNow(NoArguments());
}

void ManagementSetEnabledFunction::OnInstallPromptDone(bool did_accept) {
  if (did_accept) {
    ManagementAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->GetDelegate()
        ->EnableExtension(browser_context(), extension_id_);
    Respond(OneArgument(std::make_unique<base::Value>(true)));
  } else {
    Respond(Error(keys::kUserDidNotReEnableError));
  }

  Release();  // Balanced in Run().
}

void ManagementSetEnabledFunction::OnRequirementsChecked(
    const PreloadCheck::Errors& errors) {
  if (errors.empty()) {
    ManagementAPI::GetFactoryInstance()->Get(browser_context())->GetDelegate()->
        EnableExtension(browser_context(), extension_id_);
    Respond(NoArguments());
  } else {
    // TODO(devlin): Should we really be noisy here all the time?
    Respond(Error(keys::kMissingRequirementsError,
                  base::UTF16ToUTF8(requirements_checker_->GetErrorMessage())));
  }
}

ManagementUninstallFunctionBase::ManagementUninstallFunctionBase() {
}

ManagementUninstallFunctionBase::~ManagementUninstallFunctionBase() {
}

ExtensionFunction::ResponseAction ManagementUninstallFunctionBase::Uninstall(
    const std::string& target_extension_id,
    bool show_confirm_dialog) {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    return RespondNow(Error(keys::kNotAllowedInKioskError));

  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  target_extension_id_ = target_extension_id;
  const Extension* target_extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(target_extension_id_,
                             ExtensionRegistry::EVERYTHING);
  if (!target_extension || !target_extension->ShouldExposeViaManagementAPI()) {
    return RespondNow(Error(keys::kNoExtensionError, target_extension_id_));
  }

  ManagementPolicy* policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  if (!policy->UserMayModifySettings(target_extension, nullptr) ||
      policy->MustRemainInstalled(target_extension, nullptr)) {
    return RespondNow(Error(keys::kUserCantModifyError, target_extension_id_));
  }

  // Note: null extension() means it's WebUI.
  bool self_uninstall = extension() && extension_id() == target_extension_id_;
  // We need to show a dialog for any extension uninstalling another extension.
  show_confirm_dialog |= !self_uninstall;

  if (show_confirm_dialog && !user_gesture())
    return RespondNow(Error(keys::kGestureNeededForUninstallError));

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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
    const base::string16& error) {
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
    base::string16 utf16_error;
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

ManagementUninstallFunction::ManagementUninstallFunction() {
}

ManagementUninstallFunction::~ManagementUninstallFunction() {
}

ExtensionFunction::ResponseAction ManagementUninstallFunction::Run() {
  std::unique_ptr<management::Uninstall::Params> params(
      management::Uninstall::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool show_confirm_dialog = params->options.get() &&
                             params->options->show_confirm_dialog.get() &&
                             *params->options->show_confirm_dialog;
  return Uninstall(params->id, show_confirm_dialog);
}

ManagementUninstallSelfFunction::ManagementUninstallSelfFunction() {
}

ManagementUninstallSelfFunction::~ManagementUninstallSelfFunction() {
}

ExtensionFunction::ResponseAction ManagementUninstallSelfFunction::Run() {
  std::unique_ptr<management::UninstallSelf::Params> params(
      management::UninstallSelf::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  EXTENSION_FUNCTION_VALIDATE(extension_.get());

  bool show_confirm_dialog = params->options.get() &&
                             params->options->show_confirm_dialog.get() &&
                             *params->options->show_confirm_dialog;
  return Uninstall(extension_->id(), show_confirm_dialog);
}

ManagementCreateAppShortcutFunction::ManagementCreateAppShortcutFunction() {
}

ManagementCreateAppShortcutFunction::~ManagementCreateAppShortcutFunction() {
}

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
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    return RespondNow(Error(keys::kNotAllowedInKioskError));

  if (!user_gesture())
    return RespondNow(Error(keys::kGestureNeededForCreateAppShortcutError));

  std::unique_ptr<management::CreateAppShortcut::Params> params(
      management::CreateAppShortcut::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
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

#if defined(OS_MACOSX)
  if (!extension->is_platform_app())
    return RespondNow(Error(keys::kCreateOnlyPackagedAppShortcutMac));
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
    return RespondNow(Error(error));
  }
}

ExtensionFunction::ResponseAction ManagementSetLaunchTypeFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    return RespondNow(Error(keys::kNotAllowedInKioskError));

  if (!user_gesture())
    return RespondNow(Error(keys::kGestureNeededForSetLaunchTypeError));

  std::unique_ptr<management::SetLaunchType::Params> params(
      management::SetLaunchType::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  if (!extension)
    return RespondNow(Error(keys::kNoExtensionError, params->id));

  if (!extension->is_app())
    return RespondNow(Error(keys::kNotAnAppError, params->id));

  std::vector<management::LaunchType> available_launch_types =
      GetAvailableLaunchTypes(*extension, delegate);

  management::LaunchType app_launch_type = params->launch_type;
  if (!base::Contains(available_launch_types, app_launch_type)) {
    return RespondNow(Error(keys::kLaunchTypeNotAvailableError));
  }

  LaunchType launch_type = LAUNCH_TYPE_DEFAULT;
  switch (app_launch_type) {
    case management::LAUNCH_TYPE_OPEN_AS_PINNED_TAB:
      launch_type = LAUNCH_TYPE_PINNED;
      break;
    case management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB:
      launch_type = LAUNCH_TYPE_REGULAR;
      break;
    case management::LAUNCH_TYPE_OPEN_FULL_SCREEN:
      launch_type = LAUNCH_TYPE_FULLSCREEN;
      break;
    case management::LAUNCH_TYPE_OPEN_AS_WINDOW:
      launch_type = LAUNCH_TYPE_WINDOW;
      break;
    case management::LAUNCH_TYPE_NONE:
      NOTREACHED();
  }

  delegate->SetLaunchType(browser_context(), params->id, launch_type);

  return RespondNow(NoArguments());
}

ManagementGenerateAppForLinkFunction::ManagementGenerateAppForLinkFunction() {}

ManagementGenerateAppForLinkFunction::~ManagementGenerateAppForLinkFunction() {}

void ManagementGenerateAppForLinkFunction::FinishCreateWebApp(
    const std::string& web_app_id,
    bool install_success) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  const Extension* extension =
      registry->enabled_extensions().GetByID(web_app_id);

  // |extension| is nullptr here if install succeeds with
  // kDesktopPWAsWithoutExtensions mode enabled: there is no underlying
  // extension for |web_app_id|.
  // TODO(loyso): Rework generateAppForLink API: crbug.com/945205.
  ResponseValue response;
  if (install_success && extension) {
    response = ArgumentList(management::GenerateAppForLink::Results::Create(
        CreateExtensionInfo(nullptr, *extension, browser_context())));
  } else {
    response = Error(keys::kGenerateAppForLinkInstallError);
  }

  Respond(std::move(response));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction ManagementGenerateAppForLinkFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    return RespondNow(Error(keys::kNotAllowedInKioskError));

  if (!user_gesture())
    return RespondNow(Error(keys::kGestureNeededForGenerateAppForLinkError));

  std::unique_ptr<management::GenerateAppForLink::Params> params(
      management::GenerateAppForLink::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL launch_url(params->url);
  if (!launch_url.is_valid() || !launch_url.SchemeIsHTTPOrHTTPS()) {
    return RespondNow(Error(
        ErrorUtils::FormatErrorMessage(keys::kInvalidURLError, params->url)));
  }

  if (params->title.empty())
    return RespondNow(Error(keys::kEmptyTitleError));

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

ManagementCanInstallReplacementAndroidAppFunction::
    ManagementCanInstallReplacementAndroidAppFunction() {}

ManagementCanInstallReplacementAndroidAppFunction::
    ~ManagementCanInstallReplacementAndroidAppFunction() {}

ExtensionFunction::ResponseAction
ManagementCanInstallReplacementAndroidAppFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    return RespondNow(Error(keys::kNotAllowedInKioskError));

  if (!extension()->from_webstore()) {
    return RespondNow(
        Error(keys::kInstallReplacementAndroidAppNotFromWebstoreError));
  }

  auto* api_delegate = ManagementAPI::GetFactoryInstance()
                           ->Get(browser_context())
                           ->GetDelegate();

  DCHECK(api_delegate);

  if (!api_delegate->CanContextInstallAndroidApps(browser_context())) {
    return RespondNow(ArgumentList(
        management::CanInstallReplacementAndroidApp::Results::Create(false)));
  }

  DCHECK(ReplacementAppsInfo::HasReplacementAndroidApp(extension()));

  const std::string& package_name =
      ReplacementAppsInfo::GetReplacementAndroidApp(extension());

  api_delegate->CheckAndroidAppInstallStatus(
      package_name,
      base::BindOnce(&ManagementCanInstallReplacementAndroidAppFunction::
                         OnFinishedAndroidAppCheck,
                     this));

  // Response is sent async in FinishCheckAndroidApp().
  return RespondLater();
}

void ManagementCanInstallReplacementAndroidAppFunction::
    OnFinishedAndroidAppCheck(bool installable) {
  Respond(
      ArgumentList(management::CanInstallReplacementAndroidApp::Results::Create(
          installable)));
}

ManagementInstallReplacementAndroidAppFunction::
    ManagementInstallReplacementAndroidAppFunction() {}

ManagementInstallReplacementAndroidAppFunction::
    ~ManagementInstallReplacementAndroidAppFunction() {}

ExtensionFunction::ResponseAction
ManagementInstallReplacementAndroidAppFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    return RespondNow(Error(keys::kNotAllowedInKioskError));

  if (!extension()->from_webstore()) {
    return RespondNow(
        Error(keys::kInstallReplacementAndroidAppNotFromWebstoreError));
  }

  if (!user_gesture()) {
    return RespondNow(
        Error(keys::kGestureNeededForInstallReplacementAndroidAppError));
  }

  auto* api_delegate = ManagementAPI::GetFactoryInstance()
                           ->Get(browser_context())
                           ->GetDelegate();

  DCHECK(api_delegate);
  if (!api_delegate->CanContextInstallAndroidApps(browser_context())) {
    return RespondNow(
        Error(keys::kInstallReplacementAndroidAppInvalidContextError));
  }

  DCHECK(ReplacementAppsInfo::HasReplacementAndroidApp(extension()));

  api_delegate->InstallReplacementAndroidApp(
      ReplacementAppsInfo::GetReplacementAndroidApp(extension()),
      base::BindOnce(&ManagementInstallReplacementAndroidAppFunction::
                         OnAppInstallInitiated,
                     this));

  // Response is sent async in OnAppInstallInitiated().
  return RespondLater();
}

void ManagementInstallReplacementAndroidAppFunction::OnAppInstallInitiated(
    bool initiated) {
  if (!initiated)
    return Respond(Error(keys::kInstallReplacementAndroidAppCannotInstallApp));

  return Respond(NoArguments());
}

ManagementInstallReplacementWebAppFunction::
    ManagementInstallReplacementWebAppFunction() {}

ManagementInstallReplacementWebAppFunction::
    ~ManagementInstallReplacementWebAppFunction() {}

ExtensionFunction::ResponseAction
ManagementInstallReplacementWebAppFunction::Run() {
  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    return RespondNow(Error(keys::kNotAllowedInKioskError));

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
  ResponseValue response;
  switch (result) {
    case ManagementAPIDelegate::InstallOrLaunchWebAppResult::kSuccess:
      response = NoArguments();
      break;
    case ManagementAPIDelegate::InstallOrLaunchWebAppResult::kInvalidWebApp:
      response = Error(keys::kInstallReplacementWebAppInvalidWebAppError);
      break;
    case ManagementAPIDelegate::InstallOrLaunchWebAppResult::kUnknownError:
      response = Error(keys::kGenerateAppForLinkInstallError);
  }
  Respond(std::move(response));
}

ManagementEventRouter::ManagementEventRouter(content::BrowserContext* context)
    : browser_context_(context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

ManagementEventRouter::~ManagementEventRouter() {}

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
  if (!extension->ShouldExposeViaManagementAPI())
    return;
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  if (event_name == management::OnUninstalled::kEventName) {
    args->AppendString(extension->id());
  } else {
    args->Append(
        CreateExtensionInfo(nullptr, *extension, browser_context_).ToValue());
  }

  EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::unique_ptr<Event>(
          new Event(histogram_value, event_name, std::move(args))));
}

ManagementAPI::ManagementAPI(content::BrowserContext* context)
    : browser_context_(context),
      delegate_(ExtensionsAPIClient::Get()->CreateManagementAPIDelegate()) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, management::OnInstalled::kEventName);
  event_router->RegisterObserver(this, management::OnUninstalled::kEventName);
  event_router->RegisterObserver(this, management::OnEnabled::kEventName);
  event_router->RegisterObserver(this, management::OnDisabled::kEventName);
}

ManagementAPI::~ManagementAPI() {
}

void ManagementAPI::Shutdown() {
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
  management_event_router_.reset(new ManagementEventRouter(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
