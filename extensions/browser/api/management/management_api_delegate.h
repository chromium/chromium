// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_DELEGATE_H_

#include "base/functional/callback.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/api/management.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class Extension;
class ExtensionPrefs;
class ManagementCreateAppShortcutFunction;
class ManagementGenerateAppForLinkFunction;
class ManagementUninstallFunctionBase;

// Manages the lifetime of the install prompt.
class InstallPromptDelegate {
 public:
  virtual ~InstallPromptDelegate() {}
};

// Manages the lifetime of the uninstall prompt.
class UninstallDialogDelegate {
 public:
  virtual ~UninstallDialogDelegate() {}
};

// Manages the lifetime of the bookmark app creation.
class AppForLinkDelegate {
 public:
  virtual ~AppForLinkDelegate() {}

  virtual extensions::api::management::ExtensionInfo
  CreateExtensionInfoFromWebApp(const std::string& app_id,
                                content::BrowserContext* context) = 0;
};

class ManagementAPIDelegate {
 public:
  virtual ~ManagementAPIDelegate() {}

  enum class InstallOrLaunchWebAppResult {
    kSuccess,
    kInvalidWebApp,
    kUnknownError
  };
  using InstallOrLaunchWebAppCallback =
      base::OnceCallback<void(InstallOrLaunchWebAppResult)>;

  // Launches the app |extension|. Returns `false` if the launch was blocked due
  // to chrome apps deprecation, and `true` if it succeeded.
  virtual bool LaunchAppFunctionDelegate(
      const Extension* extension,
      content::BrowserContext* context) const = 0;

  // Forwards the call to AppLaunchInfo::GetFullLaunchURL in chrome.
  virtual GURL GetFullLaunchURL(const Extension* extension) const = 0;

  // Forwards the call to launch_util::GetLaunchType in chrome.
  virtual LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                                   const Extension* extension) const = 0;

  // Used to show a dialog prompt in chrome when management.setEnabled extension
  // function is called.
  virtual std::unique_ptr<InstallPromptDelegate> SetEnabledFunctionDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const Extension* extension,
      base::OnceCallback<void(bool)> callback) const = 0;

  // Enables the extension identified by |extension_id|.
  virtual void EnableExtension(content::BrowserContext* context,
                               const ExtensionId& extension_id) const = 0;

  // Disables the extension identified by |extension_id|. |source_extension| (if
  // specified) is the extension that originated the request.
  virtual void DisableExtension(
      content::BrowserContext* context,
      const Extension* source_extension,
      const ExtensionId& extension_id,
      disable_reason::DisableReason disable_reason) const = 0;

  // Used to show a confirmation dialog when uninstalling |target_extension|.
  virtual std::unique_ptr<UninstallDialogDelegate> UninstallFunctionDelegate(
      ManagementUninstallFunctionBase* function,
      const Extension* target_extension,
      bool show_programmatic_uninstall_ui) const = 0;

  // Uninstalls the extension.
  virtual bool UninstallExtension(content::BrowserContext* context,
                                  const std::string& transient_extension_id,
                                  UninstallReason reason,
                                  std::u16string* error) const = 0;

  // Creates an app shortcut.
  virtual bool CreateAppShortcutFunctionDelegate(
      ManagementCreateAppShortcutFunction* function,
      const Extension* extension,
      std::string* error) const = 0;

  // Forwards the call to launch_util::SetLaunchType in chrome.
  virtual void SetLaunchType(content::BrowserContext* context,
                             const ExtensionId& extension_id,
                             LaunchType launch_type) const = 0;

  // Creates a bookmark app for |launch_url|.
  virtual std::unique_ptr<AppForLinkDelegate>
  GenerateAppForLinkFunctionDelegate(
      ManagementGenerateAppForLinkFunction* function,
      content::BrowserContext* context,
      const std::string& title,
      const GURL& launch_url) const = 0;

  // Returns whether the current user type can install web apps.
  virtual bool CanContextInstallWebApps(
      content::BrowserContext* context) const = 0;

  // Installs a web app for |web_app_url| or launches if already installed.
  virtual void InstallOrLaunchReplacementWebApp(
      content::BrowserContext* context,
      const GURL& web_app_url,
      InstallOrLaunchWebAppCallback callback) const = 0;

  // Forwards the call to ExtensionIconSource::GetIconURL in chrome.
  virtual GURL GetIconURL(const Extension* extension,
                          int icon_size,
                          ExtensionIconSet::Match match,
                          bool grayscale) const = 0;

  // Returns effective update URL from ExtensionManagement.
  virtual GURL GetEffectiveUpdateURL(
      const Extension& extension,
      content::BrowserContext* context) const = 0;

  // Displays the re-enable dialog when `extension` was disabled due to the MV2
  // deprecation. Calls `done_callback` when accepted/cancelled.
  virtual void ShowMv2DeprecationReEnableDialog(
      content::BrowserContext* context,
      content::WebContents* web_contents,
      const Extension& extension,
      base::OnceCallback<void(bool)> done_callback) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_DELEGATE_H_
