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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator_id.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/language.h"

namespace blink {

Navigator::Navigator(LocalFrame* frame)
    : NavigatorLanguage(frame->GetDocument()), DOMWindowClient(frame) {}

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
  // mobile and desktop when FreezeUserAgent is enabled.
  if (GetFrame() &&
      !GetFrame()->GetSettings()->GetNavigatorPlatformOverride().IsEmpty()) {
    return GetFrame()->GetSettings()->GetNavigatorPlatformOverride();
  }

  return NavigatorID::platform();
}

String Navigator::userAgent() const {
  // If the frame is already detached it no longer has a meaningful useragent.
  if (!GetFrame() || !GetFrame()->GetPage())
    return String();

  return GetFrame()->Loader().UserAgent();
}

UserAgentMetadata Navigator::GetUserAgentMetadata() const {
  // If the frame is already detached it no longer has a meaningful useragent.
  if (!GetFrame() || !GetFrame()->GetPage())
    return blink::UserAgentMetadata();

  return GetFrame()->Loader().UserAgentMetadata();
}

bool Navigator::cookieEnabled() const {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return false;

  Settings* settings = GetFrame()->GetSettings();
  if (!settings || !settings->GetCookieEnabled())
    return false;

  return GetFrame()->GetDocument()->CookiesEnabled();
}

String Navigator::GetAcceptLanguages() {
  String accept_languages;
  if (GetFrame() && GetFrame()->GetPage()) {
    accept_languages =
        GetFrame()->GetPage()->GetChromeClient().AcceptLanguages();
  } else {
    accept_languages = DefaultLanguage();
  }

  return accept_languages;
}

void Navigator::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  NavigatorLanguage::Trace(visitor);
  DOMWindowClient::Trace(visitor);
  Supplementable<Navigator>::Trace(visitor);
}

}  // namespace blink
