// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_NAVIGATOR_ML_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_NAVIGATOR_ML_H_

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Document;
class ML;
class Navigator;

class NavigatorML final : public GarbageCollected<NavigatorML>,
                          public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorML);

 public:
  explicit NavigatorML(Navigator&);

  static const char kSupplementName[];

  // Gets or creates NavigatorML supplement on Navigator.
  static NavigatorML& From(Navigator&);

  static ML* ml(Navigator&);

  Document* GetDocument();

  void Trace(blink::Visitor*) override;

 private:
  Member<ML> ml_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_NAVIGATOR_ML_H_
