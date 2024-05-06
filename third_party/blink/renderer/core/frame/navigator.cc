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
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
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
  return settings && settings->GetCookieEnabled();
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
  Supplementable<Navigator>::Trace(visitor);
}

}  // namespace blink
