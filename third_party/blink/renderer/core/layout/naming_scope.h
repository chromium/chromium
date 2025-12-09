// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NAMING_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NAMING_SCOPE_H_

#include "base/functional/function_ref.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/core/style/style_name_scope.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// This class scopes a |name_| according to a CSS property. It represents a
// name-element pair that acts as a scope for any reference to that name within
// the subtree of that element.
//
// A CSS property (the scoping property) that wishes to restrict the visibility
// of names which are declared by some related property (the declaring property)
// and referenced by some other related property (the referencing property)
// may use this class to track the DOM subtree within which a given name should
// be visible.
//
// For example, anchor-scope (the scoping property) limits the visibility of
// of names declared by anchor-name (the declaring property) so that the search
// for a name referenced by position-anchor (the referencing property) is
// appropriately scoped.
//
// By using this class (which takes the scoping element into account) as a key
// in a map, (e.g. AnchorMap::NamedAnchorMap) we can avoid traversing references
// outside the relevant scope during lookup. E.g. AnchorMap::AnchorReference can
// avoid looking outside the relevant anchor-scope for an anchor-name.
class CORE_EXPORT NamingScope : public GarbageCollected<NamingScope> {
 public:
  NamingScope(const ScopedCSSName& name, const Element* scope_element)
      : name_(&name), scope_element_(scope_element) {}

  const AtomicString& GetName() const { return name_->GetName(); }
  const ScopedCSSName* GetScopedNameForTesting() const { return name_.Get(); }

  bool operator==(const NamingScope& other) const {
    return base::ValuesEquivalent(name_, other.name_) &&
           scope_element_ == other.scope_element_;
  }

  unsigned GetHash() const {
    unsigned hash = name_->GetHash();
    AddIntToHash(hash, blink::GetHash(scope_element_.Get()));
    return hash;
  }

  void Trace(Visitor* visitor) const;

  // Traverses the flat-tree ancestors of the specified element (including the
  // element), looking for a matching name-scoping property value (e.g.
  // anchor-scope, trigger-scope), and returns the scoping element.
  static const Element* FindScopeElement(
      const ScopedCSSName& name,
      const Element& start_element,
      base::FunctionRef<StyleNameScope(const ComputedStyle& style)> get_scope);

 private:
  Member<const ScopedCSSName> name_;
  Member<const Element> scope_element_;
};

// Allows creating a hash table of Member<NamingScope> that hashes the
// NamingScopes by value instead of address.
template <typename T>
struct NamingScopeHashTraits : MemberHashTraits<T> {
  using TraitType = typename MemberHashTraits<T>::TraitType;
  static unsigned GetHash(const TraitType& name) { return name->GetHash(); }
  static bool Equal(const TraitType& a, const TraitType& b) {
    return base::ValuesEquivalent(a, b);
  }
  // Set this flag to 'false', otherwise Equal above will see gibberish values
  // that aren't safe to call ValuesEquivalent on.
  static constexpr bool kSafeToCompareToEmptyOrDeleted = false;
};

template <>
struct HashTraits<Member<NamingScope>> : NamingScopeHashTraits<NamingScope> {};
template <>
struct HashTraits<Member<const NamingScope>>
    : NamingScopeHashTraits<const NamingScope> {};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NAMING_SCOPE_H_
