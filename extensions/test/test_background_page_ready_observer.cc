// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_background_page_ready_observer.h"

#include "base/bind.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

bool IsExtensionBackgroundPageReady(
    content::BrowserContext* browser_context,
    const extensions::ExtensionId& extension_id) {
  const auto* const extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);
  if (!extension_registry)
    return false;
  const extensions::Extension* const extension =
      extension_registry->GetInstalledExtension(extension_id);
  if (!extension)
    return false;
  auto* const extension_system =
      extensions::ExtensionSystem::Get(browser_context);
  if (!extension_system)
    return false;

  process_util::PersistentBackgroundPageState persistent_background_page_state =
      process_util::GetPersistentBackgroundPageState(*extension,
                                                     browser_context);
  switch (persistent_background_page_state) {
    // Probably erroneously, we return true for the kInvalid state, as well
    // (which corresponds to the extension not having a persistent background
    // page). This is appropriate for no background context at all, but probably
    // wrong for event pages.
    // ExtensionBackgroundPageWaiter handles this more robustly.
    case process_util::PersistentBackgroundPageState::kInvalid:
    case process_util::PersistentBackgroundPageState::kReady:
      return true;
    case process_util::PersistentBackgroundPageState::kNotReady:
      return false;
  }
}

}  // namespace

ExtensionBackgroundPageReadyObserver::ExtensionBackgroundPageReadyObserver(
    content::BrowserContext* browser_context,
    const extensions::ExtensionId& extension_id)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      notification_observer_(
          extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
          base::BindRepeating(
              &ExtensionBackgroundPageReadyObserver::IsNotificationRelevant,
              base::Unretained(this))) {}

ExtensionBackgroundPageReadyObserver::~ExtensionBackgroundPageReadyObserver() =
    default;

void ExtensionBackgroundPageReadyObserver::Wait() {
  notification_observer_.Wait();
}

bool ExtensionBackgroundPageReadyObserver::IsNotificationRelevant(
    const content::NotificationSource& source,
    const content::NotificationDetails& /*details*/) const {
  if (content::Source<const extensions::Extension>(source)->id() !=
      extension_id_) {
    return false;
  }
  // Double-check via the extension system, since the notification could be
  // for a different profile.
  return IsExtensionBackgroundPageReady(browser_context_, extension_id_);
}

}  // namespace extensions
