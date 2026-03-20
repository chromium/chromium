// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_ELEMENT_BEHAVIOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_ELEMENT_BEHAVIOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ElementInternals;
class Event;

// ElementBehavior is the base class for platform-provided behaviors
// that can be attached to custom elements via ElementInternals.
// Each subclass represents a specific set of native behaviors.
class CORE_EXPORT ElementBehavior : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~ElementBehavior() override;

  // Called when the custom element is activated. Returns true if the activation
  // was handled.
  virtual bool HandleActivation(Event& event);

  // Returns the default ARIA role for accessibility. Subclasses must
  // override this to provide appropriate roles (e.g., "button").
  virtual const AtomicString& DefaultAriaRole() const = 0;

  void Trace(Visitor* visitor) const override;

 protected:
  ElementBehavior();

 private:
  Member<ElementInternals> internals_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_ELEMENT_BEHAVIOR_H_
