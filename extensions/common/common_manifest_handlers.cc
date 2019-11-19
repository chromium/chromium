// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/common_manifest_handlers.h"

#include <memory>

#include "components/nacl/common/buildflags.h"
#include "extensions/common/api/bluetooth/bluetooth_manifest_handler.h"
#include "extensions/common/api/declarative/declarative_manifest_handler.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_handler.h"
#include "extensions/common/api/printer_provider/usb_printer_manifest_handler.h"
#include "extensions/common/api/sockets/sockets_manifest_handler.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_capabilities_handler.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/common/manifest_handlers/nacl_modules_handler.h"
#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/replacement_apps.h"
#include "extensions/common/manifest_handlers/requirements_info.h"
#include "extensions/common/manifest_handlers/sandboxed_page_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "extensions/common/manifest_url_handlers.h"

#if defined(OS_CHROMEOS)
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#endif

namespace extensions {

void RegisterCommonManifestHandlers() {
  // TODO(devlin): Pass in |registry| rather than Get()ing it.
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();

  DCHECK(!ManifestHandler::IsRegistrationFinalized());
#if defined(OS_CHROMEOS)
  registry->RegisterHandler(std::make_unique<ActionHandlersHandler>());
#endif
  registry->RegisterHandler(std::make_unique<BackgroundManifestHandler>());
  registry->RegisterHandler(std::make_unique<BluetoothManifestHandler>());
  registry->RegisterHandler(std::make_unique<ContentCapabilitiesHandler>());
  registry->RegisterHandler(std::make_unique<ContentScriptsHandler>());
  registry->RegisterHandler(std::make_unique<CSPHandler>());
  registry->RegisterHandler(
      std::make_unique<declarative_net_request::DNRManifestHandler>());
  registry->RegisterHandler(std::make_unique<DeclarativeManifestHandler>());
  registry->RegisterHandler(std::make_unique<DefaultLocaleHandler>());
  registry->RegisterHandler(std::make_unique<ExternallyConnectableHandler>());
  registry->RegisterHandler(std::make_unique<FileHandlersParser>());
  registry->RegisterHandler(std::make_unique<IconsHandler>());
  registry->RegisterHandler(std::make_unique<IncognitoHandler>());
  registry->RegisterHandler(std::make_unique<KioskModeHandler>());
  registry->RegisterHandler(std::make_unique<MimeTypesHandlerParser>());
#if BUILDFLAG(ENABLE_NACL)
  registry->RegisterHandler(std::make_unique<NaClModulesHandler>());
#endif
  registry->RegisterHandler(std::make_unique<OAuth2ManifestHandler>());
  registry->RegisterHandler(std::make_unique<OfflineEnabledHandler>());
  registry->RegisterHandler(std::make_unique<ReplacementAppsHandler>());
  registry->RegisterHandler(std::make_unique<RequirementsHandler>());
  registry->RegisterHandler(std::make_unique<SandboxedPageHandler>());
  registry->RegisterHandler(std::make_unique<SharedModuleHandler>());
  registry->RegisterHandler(std::make_unique<SocketsManifestHandler>());
  registry->RegisterHandler(std::make_unique<UpdateURLHandler>());
  registry->RegisterHandler(std::make_unique<UsbPrinterManifestHandler>());
  registry->RegisterHandler(std::make_unique<WebAccessibleResourcesHandler>());
  registry->RegisterHandler(std::make_unique<WebviewHandler>());
}

}  // namespace extensions
