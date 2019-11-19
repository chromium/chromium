// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/uninstall_ping_sender.h"

#include "base/version.h"
#include "extensions/browser/updater/update_service.h"

namespace extensions {

UninstallPingSender::UninstallPingSender(ExtensionRegistry* registry,
                                         const Filter& filter)
    : filter_(filter) {
  observer_.Add(registry);
}

UninstallPingSender::~UninstallPingSender() {}

void UninstallPingSender::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  if (filter_.Run(extension, reason) == SEND_PING) {
    UpdateService* updater = UpdateService::Get(browser_context);
    updater->SendUninstallPing(extension->id(), extension->version(), reason);
  }
}

}  // namespace extensions
