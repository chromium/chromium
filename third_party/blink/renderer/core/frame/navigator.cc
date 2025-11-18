/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA
 */

#include "third_party/blink/renderer/core/frame/navigator.h"

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator_user_activation.h"
#include "third_party/blink/renderer/core/frame/scheduling.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/geolocation/geolocation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/preferences/navigator_preferences.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/language.h"

namespace blink {

Navigator::Navigator(ExecutionContext* context) : NavigatorBase(context) {}

String Navigator::productSub() const {
  return "20030107";
}

String Navigator::vendor() const {
  // Do not change without good cause. History:
  // https://code.google.com/p/chromium/issues/detail?id=276813
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=27786
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/QrgyulnqvmE
  return "Google Inc.";
}

String Navigator::vendorSub() const {
  return "";
}

String Navigator::platform() const {
  // TODO(955620): Consider changing devtools overrides to only allow overriding
  // the platform with a frozen platform to distinguish between
  // mobile and desktop when ReduceUserAgent is enabled.
  if (!DomWindow())
    return NavigatorBase::platform();
  const String& platform_override =
      DomWindow()->GetFrame()->GetSettings()->GetNavigatorPlatformOverride();
  return platform_override.empty() ? NavigatorBase::platform()
                                   : platform_override;
}

bool Navigator::cookieEnabled() const {
  if (!DomWindow())
    return false;

  if (DomWindow()->GetStorageKey().IsThirdPartyContext()) {
    DomWindow()->CountUse(WebFeature::kNavigatorCookieEnabledThirdParty);
  }

  Settings* settings = DomWindow()->GetFrame()->GetSettings();
  bool cookie_enabled = settings && settings->GetCookieEnabled();

#if !BUILDFLAG(IS_ANDROID)
  // We don't want to print this message for WebView, and the utility is much
  // lower for all Android platforms anyway, so it seems reasonable to skip.
  if (cookie_enabled && DomWindow()->Url().IsLocalFile()) {
    DomWindow()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "While navigator.cookieEnabled does return true for this file:// "
            "URL, this is done for web compatability reasons. Cookies will not "
            "actually be stored for file:// URLs. If you want this to change "
            "please leave feedback on crbug.com/378604901."),
        /*discard_duplicates=*/true);
  }
#endif

  return cookie_enabled;
}

bool Navigator::webdriver() const {
  if (RuntimeEnabledFeatures::AutomationControlledEnabled())
    return true;

  bool automation_enabled = false;
  probe::ApplyAutomationOverride(GetExecutionContext(), automation_enabled);
  return automation_enabled;
}

String Navigator::GetAcceptLanguages() {
  if (!DomWindow())
    return DefaultLanguage();

  return DomWindow()
      ->GetFrame()
      ->GetPage()
      ->GetChromeClient()
      .AcceptLanguages();
}

void Navigator::Trace(Visitor* visitor) const {
  NavigatorBase::Trace(visitor);
  visitor->Trace(geolocation_);
  visitor->Trace(model_context_supplement_);
  visitor->Trace(navigator_preferences_);
  visitor->Trace(navigator_user_activation_);
  visitor->Trace(scheduling_);
  visitor->Trace(authentication_credentials_container_);
  visitor->Trace(battery_manager_);
  visitor->Trace(bluetooth_);
  visitor->Trace(clipboard_);
  visitor->Trace(contacts_manager_);
  visitor->Trace(cookie_deprecation_label_);
  visitor->Trace(handwriting_recognition_service_);
  visitor->Trace(ink_);
  visitor->Trace(media_devices_);
  visitor->Trace(media_session_);
  visitor->Trace(navigator_auction_);
  visitor->Trace(navigator_beacon_);
  visitor->Trace(navigator_content_utils_);
  visitor->Trace(navigator_device_posture_);
  visitor->Trace(navigator_gamepad_);
  visitor->Trace(navigator_keyboard_);
  visitor->Trace(navigator_login_);
  visitor->Trace(navigator_managed_data_);
  visitor->Trace(navigator_plugins_);
  visitor->Trace(navigator_share_);
  visitor->Trace(navigator_web_install_);
  visitor->Trace(navigator_web_midi_);
  visitor->Trace(presentation_);
  visitor->Trace(sub_apps_);
  visitor->Trace(vibration_controller_);
  visitor->Trace(virtual_keyboard_);
  visitor->Trace(window_controls_overlay_);
  visitor->Trace(xrsystem_);
}

}  // namespace blink
