// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_UPDATE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_UPDATE_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class Document;
class ComputedStyle;
class CSSAnimationUpdate;

// CSSAnimationUpdateScope applies pending animation on destruction, if it
// is the *current* scope. A CSSAnimationUpdateScope becomes the current
// scope upon construction if there isn't one already.
class CORE_EXPORT CSSAnimationUpdateScope {
  STACK_ALLOCATED();

 public:
  explicit CSSAnimationUpdateScope(Document&);
  ~CSSAnimationUpdateScope();

  class Data {
    STACK_ALLOCATED();

   public:
    // Set a pending CSSAnimationUpdate for a given Element.
    //
    // The update will be automatically applied when the owning
    // CSSAnimationUpdateScope object goes out of scope.
    void SetPendingUpdate(Element&, const CSSAnimationUpdate&);

    // When calculating transition updates, we need the old style of the element
    // to set up the transition correctly. Container queries can cause the style
    // to be calculated (and replaced on Element) multiple times before we have
    // the final after-change ComputedStyle, hence we need to store the
    // "original" old style for affected elements in order to avoid triggering
    // transitions based on some abandoned and intermediate ComputedStyle.
    //
    // This function takes the current ComputedStyle of the element, and stores
    // it as the old style. If an old style was already stored for this Element,
    // this function does nothing.
    //
    // The old styles remain until the CSSAnimationUpdateScope/Data object goes
    // out of scope.
    void StoreOldStyleIfNeeded(Element&);

    // If an old-style was previously stored using StoreOldStyleIfNeeded,
    // this function returns that ComputedStyle. Otherwise returns the
    // current ComputedStyle on the Element.
    const ComputedStyle* GetOldStyle(Element&) const;

   private:
    friend class CSSAnimationUpdateScope;
    friend class ContainerQueryTest;

    HeapHashSet<Member<Element>> elements_with_pending_updates_;
    HeapHashMap<Member<Element>, scoped_refptr<const ComputedStyle>>
        old_styles_;
  };

  static Data* CurrentData();

 private:
  Document& document_;
  // Note that |data_| is only used if the CSSAnimationUpdateScope is the
  // current scope. Otherwise it will remain empty.
  Data data_;

  void Apply();

  static CSSAnimationUpdateScope* current_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_ANIMATION_UPDATE_SCOPE_H_
