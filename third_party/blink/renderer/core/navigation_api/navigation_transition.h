// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_TRANSITION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class NavigationHistoryEntry;

class CORE_EXPORT NavigationTransition final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NavigationTransition(ScriptState*,
                       const String& navigation_type,
                       NavigationHistoryEntry* from);
  ~NavigationTransition() final = default;

  const String& navigationType() const { return navigation_type_; }
  NavigationHistoryEntry* from() { return from_; }
  ScriptPromise finished(ScriptState* script_state);

  void ResolveFinishedPromise();
  void RejectFinishedPromise(ScriptValue ex);

  void Trace(Visitor*) const final;

 private:
  using FinishedProperty =
      ScriptPromiseProperty<ToV8UndefinedGenerator, ScriptValue>;

  String navigation_type_;
  Member<NavigationHistoryEntry> from_;
  Member<FinishedProperty> finished_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_TRANSITION_H_
