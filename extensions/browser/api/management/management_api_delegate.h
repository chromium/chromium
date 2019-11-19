// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_DELEGATE_H_

#include "base/callback.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
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
class ManagementGetPermissionWarningsByManifestFunction;
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
};

class ManagementAPIDelegate {
 public:
  virtual ~ManagementAPIDelegate() {}

  using AndroidAppInstallStatusCallback = base::OnceCallback<void(bool)>;
  using InstallAndroidAppCallback = base::OnceCallback<void(bool)>;

  enum class InstallOrLaunchWebAppResult {
    kSuccess,
    kInvalidWebApp,
    kUnknownError
  };
  using InstallOrLaunchWebAppCallback =
      base::OnceCallback<void(InstallOrLaunchWebAppResult)>;

  // Launches the app |extension|.
  virtual void LaunchAppFunctionDelegate(
      const Extension* extension,
      content::BrowserContext* context) const = 0;

  // Forwards the call to AppLaunchInfo::GetFullLaunchURL in chrome.
  virtual GURL GetFullLaunchURL(const Extension* extension) const = 0;

  // Forwards the call to launch_util::GetLaunchType in chrome.
  virtual LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                                   const Extension* extension) const = 0;

  // Parses the manifest and calls back the
  // ManagementGetPermissionWarningsByManifestFunction.
  virtual void GetPermissionWarningsByManifestFunctionDelegate(
      ManagementGetPermissionWarningsByManifestFunction* function,
      const std::string& manifest_str) const = 0;

  // Used to show a dialog prompt in chrome when management.setEnabled extension
  // function is called.
  virtual std::unique_ptr<InstallPromptDelegate> SetEnabledFunctionDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Callback<void(bool)>& callback) const = 0;

  // Enables the extension identified by |extension_id|.
  virtual void EnableExtension(content::BrowserContext* context,
                               const std::string& extension_id) const = 0;

  // Disables the extension identified by |extension_id|. |source_extension| (if
  // specified) is the extension that originated the request.
  virtual void DisableExtension(
      content::BrowserContext* context,
      const Extension* source_extension,
      const std::string& extension_id,
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
                                  base::string16* error) const = 0;

  // Creates an app shortcut.
  virtual bool CreateAppShortcutFunctionDelegate(
      ManagementCreateAppShortcutFunction* function,
      const Extension* extension,
      std::string* error) const = 0;

  // Forwards the call to launch_util::SetLaunchType in chrome.
  virtual void SetLaunchType(content::BrowserContext* context,
                             const std::string& extension_id,
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

  // Returns whether arc apps can be installed in the given |context|.
  virtual bool CanContextInstallAndroidApps(
      content::BrowserContext* context) const = 0;

  // Checks the installation status of |package_name|.
  virtual void CheckAndroidAppInstallStatus(
      const std::string& package_name,
      AndroidAppInstallStatusCallback callback) const = 0;

  // Installs an Arc app for |package_name|.
  virtual void InstallReplacementAndroidApp(
      const std::string& package_name,
      InstallAndroidAppCallback callback) const = 0;

  // Forwards the call to ExtensionIconSource::GetIconURL in chrome.
  virtual GURL GetIconURL(const Extension* extension,
                          int icon_size,
                          ExtensionIconSet::MatchType match,
                          bool grayscale) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_DELEGATE_H_
