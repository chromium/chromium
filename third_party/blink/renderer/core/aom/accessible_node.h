// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class Element;
class QualifiedName;

// All of the properties of AccessibleNode that have type "string".
enum class AOMStringProperty {
  kAutocomplete,
  kAriaBrailleLabel,
  kAriaBrailleRoleDescription,
  kChecked,
  kColIndexText,
  kCurrent,
  kDescription,
  kHasPopup,
  kInvalid,
  kKeyShortcuts,
  kLabel,
  kLive,
  kOrientation,
  kPlaceholder,
  kPressed,
  kRelevant,
  kRole,
  kRoleDescription,
  kRowIndexText,
  kSort,
  kValueText,
  kVirtualContent
};

enum class AOMRelationProperty {
  kActiveDescendant,
};

enum class AOMRelationListProperty {
  kDescribedBy,
  kDetails,
  kControls,
  kErrorMessage,
  kFlowTo,
  kLabeledBy,
  kOwns,
};

class AccessibleNode;

class CORE_EXPORT AccessibleNode {
 public:
  // Returns the value of the given string property if the
  // Element has an AccessibleNode, otherwise returns the equivalent
  // ARIA attribute.
  static const AtomicString& GetPropertyOrARIAAttributeValue(
      Element* element,
      AOMRelationProperty property);

  // Returns the equivalent ARIA attribute.
  static Element* GetPropertyOrARIAAttribute(Element*, AOMRelationProperty);

  // Returns true with the equivalent ARIA attribute or false if not present.
  static bool GetPropertyOrARIAAttribute(Element*,
                                         AOMRelationListProperty,
                                         HeapVector<Member<Element>>&);

 private:
  static const AtomicString& GetElementOrInternalsARIAAttribute(
      Element& element,
      const QualifiedName& attribute);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_H_
