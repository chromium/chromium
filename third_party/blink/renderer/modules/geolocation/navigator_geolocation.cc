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
 *  MA  02110-1301 USA
 */

#include "third_party/blink/renderer/modules/geolocation/navigator_geolocation.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/geolocation/geolocation.h"

namespace blink {

NavigatorGeolocation::NavigatorGeolocation(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
const char NavigatorGeolocation::kSupplementName[] = "NavigatorGeolocation";

NavigatorGeolocation& NavigatorGeolocation::From(Navigator& navigator) {
  NavigatorGeolocation* supplement =
      Supplement<Navigator>::From<NavigatorGeolocation>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorGeolocation>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

Geolocation* NavigatorGeolocation::geolocation(Navigator& navigator) {
  return NavigatorGeolocation::From(navigator).geolocation();
}

Geolocation* NavigatorGeolocation::geolocation() {
  if (!geolocation_ && GetSupplementable()->GetFrame()) {
    geolocation_ =
        Geolocation::Create(GetSupplementable()->GetFrame()->GetDocument());
  }
  return geolocation_;
}

void NavigatorGeolocation::Trace(blink::Visitor* visitor) {
  visitor->Trace(geolocation_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
