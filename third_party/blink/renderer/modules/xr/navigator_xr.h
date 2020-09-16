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
class XRSystem;

class MODULES_EXPORT NavigatorXR final : public GarbageCollected<NavigatorXR>,
                                         public Supplement<Navigator> {
 public:
  static const char kSupplementName[];

  static NavigatorXR* From(Document&);
  static NavigatorXR& From(Navigator&);

  // Allows us to check whether |Document| has a NavigatorXR, without triggering
  // its lazy creation.
  static bool AlreadyExists(Document&);

  explicit NavigatorXR(Navigator&);

  static XRSystem* xr(Navigator&);
  XRSystem* xr();

  void Trace(Visitor*) const override;

 private:
  Document* GetDocument();

  Member<XRSystem> xr_;

  // Gates metrics collection once per local DOM window frame.
  bool did_log_navigator_xr_ = false;

  DISALLOW_COPY_AND_ASSIGN(NavigatorXR);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_NAVIGATOR_XR_H_
