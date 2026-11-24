// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_pseudo_element.h"

#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

const AtomicString& TargetArgument(const StyleRequest& request) {
  if (request.search_text_request == StyleRequest::kCurrent) {
    static const base::NoDestructor<AtomicString> current(
        AtomicString::FromLiteral<"current">());
    return *current;
  }
  return request.pseudo_argument;
}

bool IsRegistered(const Element& element, const AtomicString& name) {
  const HighlightRegistry* registry =
      HighlightRegistry::GetHighlightRegistry(&element);
  if (!registry || name.IsNull()) {
    return false;
  }
  return registry->GetHighlights()
             .Find<HighlightRegistryMapEntryNameTranslator>(name) !=
         registry->GetHighlights().end();
}

bool CanHaveTarget(const Element& element, const StyleRequest& request) {
  return RuntimeEnabledFeatures::CSSCustomHighlightExtensionsEnabled() &&
         IsHighlightPseudoElement(request.pseudo_id) &&
         (request.pseudo_id != kPseudoIdHighlight ||
          (request.pseudo_argument != CSSSelector::UniversalSelectorAtom() &&
           IsRegistered(element, request.pseudo_argument)));
}

}  // namespace

HighlightPseudoElement::HighlightPseudoElement(Element& origin,
                                               PseudoId pseudo,
                                               const AtomicString& argument)
    : PseudoElement(&origin, pseudo, argument) {}

bool HighlightPseudoElement::LayoutObjectIsNeeded(const DisplayStyle&) const {
  return false;
}

PseudoElement* HighlightPseudoElement::FindTarget(const Element& element,
                                                  const StyleRequest& request) {
  if (!CanHaveTarget(element, request)) {
    return nullptr;
  }
  return element.GetPseudoElement(request.pseudo_id, TargetArgument(request));
}

PseudoElement* HighlightPseudoElement::EnsureTarget(
    Element& element,
    const StyleRequest& request) {
  if (!CanHaveTarget(element, request)) {
    return nullptr;
  }
  const AtomicString& argument = TargetArgument(request);
  if (auto* target = element.GetPseudoElement(request.pseudo_id, argument)) {
    return target;
  }
  auto* target = MakeGarbageCollected<HighlightPseudoElement>(
      element, request.pseudo_id, argument);
  element.SetPseudoElement(request.pseudo_id, target, argument);
  return target;
}

}  // namespace blink
