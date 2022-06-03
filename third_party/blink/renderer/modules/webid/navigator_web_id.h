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
class WebId;

class NavigatorWebId final : public GarbageCollected<NavigatorWebId>,
                             public Supplement<Navigator> {
 public:
  static const char kSupplementName[];

  // Gets, or creates, NavigatorID supplement on Navigator.
  // See platform/Supplementable.h
  static NavigatorWebId& From(Navigator&);

  static WebId* id(Navigator&);
  WebId* id();

  void Trace(Visitor*) const override;

  explicit NavigatorWebId(Navigator&);

 private:
  Member<WebId> web_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_NAVIGATOR_WEB_ID_H_
