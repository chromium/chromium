// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_util.h"

#include "base/check.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"

namespace extensions {
namespace process_util {

PersistentBackgroundPageState GetPersistentBackgroundPageState(
    const Extension& extension,
    content::BrowserContext* browser_context) {
  // If the extension doesn't have a persistent background page, it can never
  // be ready.
  if (!BackgroundInfo::HasPersistentBackgroundPage(&extension)) {
    return PersistentBackgroundPageState::kInvalid;
  }

  content::BrowserContext* browser_context_to_use = browser_context;
  if (browser_context->IsOffTheRecord()) {
    // Sanity checks: First check that the extension supports running in
    // incognito, according to its manifest.
    DCHECK(IncognitoInfo::IsIncognitoAllowed(&extension))
        << "Can't use an incognito browser context for an extension that "
           "doesn't suppport incognito!";
    // Then, check that the user enabled the extension in incognito.
    DCHECK(util::IsIncognitoEnabled(extension.id(), browser_context))
        << "Can't use an incognito browser context for an extension that isn't "
           "allowed to run in incognito!";

    // If the extension runs in spanning mode, the background page will be
    // associated with the on-the-record context.
    if (!IncognitoInfo::IsSplitMode(&extension)) {
      browser_context_to_use =
          ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context);
    }
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context_to_use);
  DCHECK(process_manager);

  ExtensionHost* background_host =
      process_manager->GetBackgroundHostForExtension(extension.id());

  if (!background_host || !background_host->document_element_available()) {
    return PersistentBackgroundPageState::kNotReady;
  }

  return PersistentBackgroundPageState::kReady;
}

}  // namespace process_util
}  // namespace extensions
