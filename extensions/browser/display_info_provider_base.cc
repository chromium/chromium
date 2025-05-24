// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/display_info_provider_base.h"

#include "base/values.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

DisplayInfoProviderBase::DisplayInfoProviderBase(display::Screen* screen)
    : DisplayInfoProvider(screen) {}
DisplayInfoProviderBase::~DisplayInfoProviderBase() = default;

void DisplayInfoProviderBase::DispatchOnDisplayChangedEvent() {
  // This function will dispatch the OnDisplayChangedEvent to both on-the-record
  // and off-the-record profiles. This allows extensions running in incognito
  // to be notified mirroring is enabled / disabled, which allows the Virtual
  // keyboard on ChromeOS to correctly disable key highlighting when typing
  // passwords on the login page (crbug.com/40568214)
  ExtensionsBrowserClient::Get()->BroadcastEventToRenderers(
      events::SYSTEM_DISPLAY_ON_DISPLAY_CHANGED,
      extensions::api::system_display::OnDisplayChanged::kEventName,
      base::Value::List(), /*dispatch_to_off_the_record_profiles=*/true);
}

}  // namespace extensions
