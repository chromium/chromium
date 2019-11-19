/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_NAVIGATOR_GEOLOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_NAVIGATOR_GEOLOCATION_H_

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Geolocation;
class Navigator;

class NavigatorGeolocation final
    : public GarbageCollected<NavigatorGeolocation>,
      public Supplement<Navigator>,
      public NameClient {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorGeolocation);

 public:
  static const char kSupplementName[];

  static NavigatorGeolocation& From(Navigator&);
  static Geolocation* geolocation(Navigator&);
  Geolocation* geolocation();

  explicit NavigatorGeolocation(Navigator&);

  void Trace(blink::Visitor*) override;
  const char* NameInHeapSnapshot() const override {
    return "NavigatorGeolocation";
  }

 private:
  Member<Geolocation> geolocation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_NAVIGATOR_GEOLOCATION_H_
