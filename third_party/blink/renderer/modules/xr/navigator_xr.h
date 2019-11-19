// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_NAVIGATOR_XR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_NAVIGATOR_XR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Document;
class XR;

class MODULES_EXPORT NavigatorXR final : public GarbageCollected<NavigatorXR>,
                                         public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorXR);

 public:
  static const char kSupplementName[];

  static NavigatorXR* From(Document&);
  static NavigatorXR& From(Navigator&);

  explicit NavigatorXR(Navigator&);

  static XR* xr(Navigator&);
  XR* xr();

  void Trace(blink::Visitor*) override;

 private:
  Document* GetDocument();

  Member<XR> xr_;

  // Gates metrics collection once per local DOM window frame.
  bool did_log_navigator_xr_ = false;

  DISALLOW_COPY_AND_ASSIGN(NavigatorXR);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_NAVIGATOR_XR_H_
