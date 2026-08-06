// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_ui_util.h"

#include <optional>

#include "base/check.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"

namespace extensions::mime_handler {

const Extension* GetTopLevelMimeHandlerExtension(
    content::WebContents& web_contents) {
  auto* manager = MimeHandlerStreamManager::FromWebContents(&web_contents);
  if (!manager) {
    return nullptr;
  }
  std::optional<ExtensionId> id = manager->GetTopLevelHandlerExtensionId();
  if (!id) {
    return nullptr;
  }
  auto* registry = ExtensionRegistry::Get(web_contents.GetBrowserContext());
  CHECK(registry);
  // The stream can transiently outlive the extension's enabled state:
  // `MimeHandlerStreamManager` erases streams in `OnExtensionUnloaded()`, but
  // an unloading extension leaves the enabled set before `ExtensionRegistry`
  // observers run, and an earlier observer can synchronously reach this code
  // (e.g. by starting a navigation) within that window. Treat the mid-unload
  // state as "no handler".
  const Extension* extension = registry->enabled_extensions().GetByID(*id);
  if (!extension) {
    return nullptr;
  }
  // Allowlisted plugin extensions never relabel the chip. The allowlist
  // contains built-in PDF and QuickOffice variants, so this single filter
  // covers both categories (see `kMIMETypeHandlersAllowlist`).
  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  if (handler && handler->IsPluginExtension()) {
    return nullptr;
  }
  return extension;
}

}  // namespace extensions::mime_handler
