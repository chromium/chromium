// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_NAVIGATOR_WEB_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_NAVIGATOR_WEB_ID_H_

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class WebID;

class NavigatorWebID final : public GarbageCollected<NavigatorWebID>,
                             public Supplement<Navigator> {
 public:
  static const char kSupplementName[];

  // Gets, or creates, NavigatorID supplement on Navigator.
  // See platform/Supplementable.h
  static NavigatorWebID& From(Navigator&);

  static WebID* id(Navigator&);
  WebID* id();

  void Trace(Visitor*) const override;

  explicit NavigatorWebID(Navigator&);

 private:
  Member<WebID> web_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_NAVIGATOR_WEB_ID_H_
