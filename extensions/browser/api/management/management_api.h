// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_H_
#define EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/api/management/management_api_delegate.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/preload_check.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions {

class ExtensionRegistry;
class RequirementsChecker;

class ManagementGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getAll", MANAGEMENT_GETALL)

 protected:
  ~ManagementGetAllFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ManagementGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.get", MANAGEMENT_GET)

 protected:
  ~ManagementGetFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ManagementGetSelfFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getSelf", MANAGEMENT_GETSELF)

 protected:
  ~ManagementGetSelfFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ManagementGetPermissionWarningsByIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getPermissionWarningsById",
                             MANAGEMENT_GETPERMISSIONWARNINGSBYID)

 protected:
  ~ManagementGetPermissionWarningsByIdFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ManagementGetPermissionWarningsByManifestFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getPermissionWarningsByManifest",
                             MANAGEMENT_GETPERMISSIONWARNINGSBYMANIFEST)

  // Called when manifest parsing is finished.
  void OnParse(data_decoder::DataDecoder::ValueOrError result);

 protected:
  ~ManagementGetPermissionWarningsByManifestFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ManagementLaunchAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.launchApp", MANAGEMENT_LAUNCHAPP)

 protected:
  ~ManagementLaunchAppFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ManagementSetEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.setEnabled", MANAGEMENT_SETENABLED)

  ManagementSetEnabledFunction();

 protected:
  ~ManagementSetEnabledFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnInstallPromptDone(bool did_accept);

  void OnRequirementsChecked(const PreloadCheck::Errors& errors);

  std::string extension_id_;

  std::unique_ptr<InstallPromptDelegate> install_prompt_;

  std::unique_ptr<RequirementsChecker> requirements_checker_;
};

class ManagementUninstallFunctionBase : public ExtensionFunction {
 public:
  ManagementUninstallFunctionBase();

  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const base::string16& error);

 protected:
  ~ManagementUninstallFunctionBase() override;
  ResponseAction Uninstall(const std::string& extension_id,
                           bool show_confirm_dialog);

 private:
  // Uninstalls the extension without showing the dialog.
  void UninstallExtension();

  // Finishes and responds to the extension.
  void Finish(bool did_start_uninstall, const std::string& error);

  std::string target_extension_id_;

  std::unique_ptr<UninstallDialogDelegate> uninstall_dialog_;
};

class ManagementUninstallFunction : public ManagementUninstallFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("management.uninstall", MANAGEMENT_UNINSTALL)
  ManagementUninstallFunction();

 private:
  ~ManagementUninstallFunction() override;
  ResponseAction Run() override;
};

class ManagementUninstallSelfFunction : public ManagementUninstallFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("management.uninstallSelf",
                             MANAGEMENT_UNINSTALLSELF)
  ManagementUninstallSelfFunction();

 private:
  ~ManagementUninstallSelfFunction() override;
  ResponseAction Run() override;
};

class ManagementCreateAppShortcutFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.createAppShortcut",
                             MANAGEMENT_CREATEAPPSHORTCUT)

  ManagementCreateAppShortcutFunction();

  void OnCloseShortcutPrompt(bool created);

  static void SetAutoConfirmForTest(bool should_proceed);

 protected:
  ~ManagementCreateAppShortcutFunction() override;

  ResponseAction Run() override;
};

class ManagementSetLaunchTypeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.setLaunchType",
                             MANAGEMENT_SETLAUNCHTYPE)

 protected:
  ~ManagementSetLaunchTypeFunction() override {}

  ResponseAction Run() override;
};

class ManagementGenerateAppForLinkFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.generateAppForLink",
                             MANAGEMENT_GENERATEAPPFORLINK)

  ManagementGenerateAppForLinkFunction();

  void FinishCreateWebApp(const std::string& web_app_id, bool install_success);

 protected:
  ~ManagementGenerateAppForLinkFunction() override;

  ResponseAction Run() override;

 private:
  std::unique_ptr<AppForLinkDelegate> app_for_link_delegate_;
};

class ManagementCanInstallReplacementAndroidAppFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.canInstallReplacementAndroidApp",
                             MANAGEMENT_CANINSTALLREPLACEMENTANDROIDAPP)

  ManagementCanInstallReplacementAndroidAppFunction();

 protected:
  ~ManagementCanInstallReplacementAndroidAppFunction() override;

  ResponseAction Run() override;

 private:
  void OnFinishedAndroidAppCheck(bool result);
};

class ManagementInstallReplacementAndroidAppFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.installReplacementAndroidApp",
                             MANAGEMENT_INSTALLREPLACEMENTANDROIDAPP)

  ManagementInstallReplacementAndroidAppFunction();

 protected:
  ~ManagementInstallReplacementAndroidAppFunction() override;

  ResponseAction Run() override;

 private:
  void OnAppInstallInitiated(bool installable);
};

class ManagementInstallReplacementWebAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.installReplacementWebApp",
                             MANAGEMENT_INSTALLREPLACEMENTWEBAPP)

  ManagementInstallReplacementWebAppFunction();

 protected:
  ~ManagementInstallReplacementWebAppFunction() override;

  ResponseAction Run() override;

 private:
  void FinishResponse(
      ManagementAPIDelegate::InstallOrLaunchWebAppResult result);
};

class ManagementEventRouter : public ExtensionRegistryObserver {
 public:
  explicit ManagementEventRouter(content::BrowserContext* context);
  ~ManagementEventRouter() override;

 private:
  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // Dispatches management api events to listening extensions.
  void BroadcastEvent(const Extension* extension,
                      events::HistogramValue histogram_value,
                      const char* event_name);

  content::BrowserContext* browser_context_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ManagementEventRouter);
};

class ManagementAPI : public BrowserContextKeyedAPI,
                      public EventRouter::Observer {
 public:
  explicit ManagementAPI(content::BrowserContext* context);
  ~ManagementAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ManagementAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // Returns the ManagementAPI delegate.
  const ManagementAPIDelegate* GetDelegate() const { return delegate_.get(); }

 private:
  friend class BrowserContextKeyedAPIFactory<ManagementAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ManagementAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<ManagementEventRouter> management_event_router_;

  std::unique_ptr<ManagementAPIDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ManagementAPI);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_H_
