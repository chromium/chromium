// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_INFERENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_INFERENCE_H_

#include <cstdint>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Document;
class Element;

enum class CSSToggleRole : uint8_t {
  kNone,
  kAccordion,
  kAccordionItem,
  kAccordionItemButton,
  kButton,
  kButtonWithPopup,
  kCheckbox,
  kCheckboxGroup,
  kDisclosure,
  kDisclosureButton,
  kListbox,
  kListboxItem,
  kRadioGroup,
  kRadioItem,
  kTab,
  kTabContainer,
  kTabPanel,
  kTree,
  kTreeGroup,
  kTreeItem,
};

/**
 * This is a prototype of the inference algorithm being designed in
 * https://github.com/tabatkins/css-toggle/issues/41 .  Its purpose is
 * to infer information that can be relatively easily mapped to ARIA
 * roles and properties based on CSS Toggle properties (toggle-root,
 * toggle-trigger, toggle-group, and toggle-visibility) and the DOM tree
 * relationships of the elements with those properties.  It can infer
 * roles on elements that lack those properties; for example, it might
 * infer a role for an element that is the parent of a set of elements
 * with those properties.
 *
 * The output of the algorithm should be a mapping from DOM element to
 * its inferred role and properties (if any).  (When toggles are not
 * used, inferred roles and properties should not be produced.)
 *
 * These outputs are intended to be used both for ARIA roles/properties
 * and for default keyboard behaviors given to these elements.
 *
 * This implementation is intended as a prototype and its performance
 * characteristics are not intended to be usable in a shipping
 * implementation.  A sufficiently performant implementation may need to
 * have the toggles code maintain data structures that represent the
 * relationships between the toggles (rather than the current code that
 * searches the tree when necessary).
 */

class CORE_EXPORT CSSToggleInference final
    : public GarbageCollected<CSSToggleInference> {
 public:
  explicit CSSToggleInference(Document* document) : document_(document) {}

  void Trace(Visitor* visitor) const;

  void MarkNeedsRebuild() { needs_rebuild_ = true; }

  // This returns role information for the element that is inferred from
  // the patterns of using CSS toggles (see comments above describing
  // this class).
  //
  // These roles have some relationship to ARIA roles but are not the
  // same.
  //
  // This role information is somewhat expensive to rebuild, and is
  // information that does *not* change when toggle state changes.
  CSSToggleRole RoleForElement(const blink::Element* element);

  // Return the toggle name associated with an element's role.
  //
  // ToggleNameForElement should return g_null_atom in exactly the same
  // cases that RoleForElement returns CSSToggleRole::kNone or
  // CSSToggleRole::kTree.
  AtomicString ToggleNameForElement(const blink::Element* element);

  // TODO(https://crbug.com/1250716): Add a separate API here for
  // property (e.g., state) information that *does* sometimes change
  // when toggle state is changed.

 private:
  void RebuildIfNeeded();
  void Rebuild();

  struct ElementData {
    CSSToggleRole role;
    AtomicString toggle_name;
  };

  bool needs_rebuild_ = true;
  Member<Document> document_;
  HeapHashMap<Member<const Element>, ElementData> element_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_INFERENCE_H_
