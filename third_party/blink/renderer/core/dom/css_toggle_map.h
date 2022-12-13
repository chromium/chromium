// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_MAP_H_

#include "third_party/blink/renderer/bindings/core/v8/maplike.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_sync_iterator_css_toggle_map.h"
#include "third_party/blink/renderer/core/dom/css_toggle.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class Element;
class ExceptionState;
class ToggleRootList;

// Represents the set of toggles on an element.
using ToggleMap = HeapHashMap<AtomicString, Member<CSSToggle>>;

using CSSToggleMapMaplike = MaplikeReadAPIs<CSSToggleMap>;

class CORE_EXPORT CSSToggleMap : public ScriptWrappable,
                                 public CSSToggleMapMaplike,
                                 public ElementRareDataField {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CSSToggleMap(Element* owner_element);

  ToggleMap& Toggles() { return toggles_; }
  Element* OwnerElement() const { return owner_element_; }
  // Create any toggles specified by 'toggle-root' that don't already exist on
  // the element.
  void CreateToggles(const ToggleRootList* toggle_roots);

  void Trace(Visitor* visitor) const override;

  CSSToggleMap* set(const AtomicString& key,
                    CSSToggle* value,
                    ExceptionState& exception_state);
  void clearForBinding(ScriptState*, ExceptionState&);
  bool deleteForBinding(ScriptState*, const AtomicString&, ExceptionState&);
  wtf_size_t size() const { return toggles_.size(); }

 private:
  bool GetMapEntry(ScriptState*,
                   const String& key,
                   CSSToggle*& value,
                   ExceptionState&) final;
  CSSToggleMapMaplike::IterationSource* CreateIterationSource(
      ScriptState*,
      ExceptionState&) final;

  class IterationSource final : public CSSToggleMapMaplike::IterationSource {
   public:
    explicit IterationSource(const CSSToggleMap& toggle_map);

    bool FetchNextItem(ScriptState*,
                       String&,
                       CSSToggle*&,
                       ExceptionState&) override;

    void Trace(blink::Visitor*) const override;

   private:
    wtf_size_t index_ = 0;
    HeapVector<Member<CSSToggle>> toggles_snapshot_;
  };

  Member<Element> owner_element_;
  ToggleMap toggles_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_MAP_H_
