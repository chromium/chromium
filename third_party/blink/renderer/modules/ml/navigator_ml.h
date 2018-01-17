// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorML_h
#define NavigatorML_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class ML;
class Navigator;

class NavigatorML final : public GarbageCollected<NavigatorML>,
                          public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorML);

 public:
  // Gets, or creates, NavigatorML supplement on Navigator.
  static NavigatorML& From(Navigator&);

  static ML* ml(Navigator&);

  Document* GetDocument();

  void Trace(blink::Visitor*) override;
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

 private:
  explicit NavigatorML(Navigator&);
  static const char* SupplementName();

  TraceWrapperMember<ML> ml_;
};

}  // namespace blink

#endif  // NavigatorML_h
