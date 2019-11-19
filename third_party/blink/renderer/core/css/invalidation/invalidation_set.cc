/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static const unsigned char* g_tracing_enabled = nullptr;

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED( \
    element, reason, invalidationSet, singleSelectorPart)             \
  if (UNLIKELY(*g_tracing_enabled))                                   \
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(                \
        element, reason, invalidationSet, singleSelectorPart);

// static
void InvalidationSetDeleter::Destruct(const InvalidationSet* obj) {
  obj->Destroy();
}

void InvalidationSet::CacheTracingFlag() {
  g_tracing_enabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
}

InvalidationSet::InvalidationSet(InvalidationType type)
    : type_(static_cast<unsigned>(type)),
      invalidates_self_(false),
      is_alive_(true) {}

bool InvalidationSet::InvalidatesElement(Element& element) const {
  if (invalidation_flags_.WholeSubtreeInvalid())
    return true;

  if (HasTagNames() && HasTagName(element.LocalNameForSelectorMatching())) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
        element, kInvalidationSetMatchedTagName, *this,
        element.LocalNameForSelectorMatching());
    return true;
  }

  if (element.HasID() && HasIds() && HasId(element.IdForStyleResolution())) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
        element, kInvalidationSetMatchedId, *this,
        element.IdForStyleResolution());
    return true;
  }

  if (element.HasClass() && HasClasses()) {
    if (StringImpl* class_name = FindAnyClass(element)) {
      TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
          element, kInvalidationSetMatchedClass, *this, String(class_name));
      return true;
    }
  }

  if (element.hasAttributes() && HasAttributes()) {
    if (StringImpl* attribute = FindAnyAttribute(element)) {
      TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
          element, kInvalidationSetMatchedAttribute, *this, String(attribute));
      return true;
    }
  }

  if (element.HasPart() && invalidation_flags_.InvalidatesParts()) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
        element, kInvalidationSetMatchedPart, *this, "");
    return true;
  }

  return false;
}

bool InvalidationSet::InvalidatesTagName(Element& element) const {
  if (HasTagNames() && HasTagName(element.LocalNameForSelectorMatching())) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART_IF_ENABLED(
        element, kInvalidationSetMatchedTagName, *this,
        element.LocalNameForSelectorMatching());
    return true;
  }

  return false;
}

void InvalidationSet::Combine(const InvalidationSet& other) {
  CHECK(is_alive_);
  CHECK(other.is_alive_);
  CHECK_EQ(GetType(), other.GetType());

  if (IsSelfInvalidationSet()) {
    // We should never modify the SelfInvalidationSet singleton. When
    // aggregating the contents from another invalidation set into an
    // invalidation set which only invalidates self, we instantiate a new
    // DescendantInvalidation set before calling Combine(). We still may end up
    // here if we try to combine two references to the singleton set.
    DCHECK(other.IsSelfInvalidationSet());
    return;
  }

  CHECK_NE(&other, this);

  if (auto* invalidation_set = DynamicTo<SiblingInvalidationSet>(this)) {
    SiblingInvalidationSet& siblings = *invalidation_set;
    const SiblingInvalidationSet& other_siblings =
        To<SiblingInvalidationSet>(other);

    siblings.UpdateMaxDirectAdjacentSelectors(
        other_siblings.MaxDirectAdjacentSelectors());
    if (other_siblings.SiblingDescendants())
      siblings.EnsureSiblingDescendants().Combine(
          *other_siblings.SiblingDescendants());
    if (other_siblings.Descendants())
      siblings.EnsureDescendants().Combine(*other_siblings.Descendants());
  }

  if (other.InvalidatesSelf()) {
    SetInvalidatesSelf();
    if (other.IsSelfInvalidationSet())
      return;
  }

  // No longer bother combining data structures, since the whole subtree is
  // deemed invalid.
  if (WholeSubtreeInvalid())
    return;

  if (other.WholeSubtreeInvalid()) {
    SetWholeSubtreeInvalid();
    return;
  }

  if (other.CustomPseudoInvalid())
    SetCustomPseudoInvalid();

  if (other.TreeBoundaryCrossing())
    SetTreeBoundaryCrossing();

  if (other.InsertionPointCrossing())
    SetInsertionPointCrossing();

  if (other.InvalidatesSlotted())
    SetInvalidatesSlotted();

  if (other.InvalidatesParts())
    SetInvalidatesParts();

  for (const auto& class_name : other.Classes())
    AddClass(class_name);

  for (const auto& id : other.Ids())
    AddId(id);

  for (const auto& tag_name : other.TagNames())
    AddTagName(tag_name);

  for (const auto& attribute : other.Attributes())
    AddAttribute(attribute);
}

void InvalidationSet::Destroy() const {
  if (auto* invalidation_set = DynamicTo<DescendantInvalidationSet>(this))
    delete invalidation_set;
  else
    delete To<SiblingInvalidationSet>(this);
}

void InvalidationSet::ClearAllBackings() {
  classes_.Clear(backing_flags_);
  ids_.Clear(backing_flags_);
  tag_names_.Clear(backing_flags_);
  attributes_.Clear(backing_flags_);
}

bool InvalidationSet::HasEmptyBackings() const {
  return classes_.IsEmpty(backing_flags_) && ids_.IsEmpty(backing_flags_) &&
         tag_names_.IsEmpty(backing_flags_) &&
         attributes_.IsEmpty(backing_flags_);
}

StringImpl* InvalidationSet::FindAnyClass(Element& element) const {
  const SpaceSplitString& class_names = element.ClassNames();
  wtf_size_t size = class_names.size();
  if (StringImpl* string_impl = classes_.GetStringImpl(backing_flags_)) {
    for (wtf_size_t i = 0; i < size; ++i) {
      if (Equal(string_impl, class_names[i].Impl()))
        return string_impl;
    }
  }
  if (const HashSet<AtomicString>* set = classes_.GetHashSet(backing_flags_)) {
    for (wtf_size_t i = 0; i < size; ++i) {
      auto item = set->find(class_names[i]);
      if (item != set->end())
        return item->Impl();
    }
  }
  return nullptr;
}

StringImpl* InvalidationSet::FindAnyAttribute(Element& element) const {
  if (StringImpl* string_impl = attributes_.GetStringImpl(backing_flags_)) {
    if (element.HasAttributeIgnoringNamespace(AtomicString(string_impl)))
      return string_impl;
  }
  if (const HashSet<AtomicString>* set =
          attributes_.GetHashSet(backing_flags_)) {
    for (const auto& attribute : *set) {
      if (element.HasAttributeIgnoringNamespace(attribute))
        return attribute.Impl();
    }
  }
  return nullptr;
}

void InvalidationSet::AddClass(const AtomicString& class_name) {
  if (WholeSubtreeInvalid())
    return;
  CHECK(!class_name.IsEmpty());
  classes_.Add(backing_flags_, class_name);
}

void InvalidationSet::AddId(const AtomicString& id) {
  if (WholeSubtreeInvalid())
    return;
  CHECK(!id.IsEmpty());
  ids_.Add(backing_flags_, id);
}

void InvalidationSet::AddTagName(const AtomicString& tag_name) {
  if (WholeSubtreeInvalid())
    return;
  CHECK(!tag_name.IsEmpty());
  tag_names_.Add(backing_flags_, tag_name);
}

void InvalidationSet::AddAttribute(const AtomicString& attribute) {
  if (WholeSubtreeInvalid())
    return;
  CHECK(!attribute.IsEmpty());
  attributes_.Add(backing_flags_, attribute);
}

void InvalidationSet::SetWholeSubtreeInvalid() {
  if (invalidation_flags_.WholeSubtreeInvalid())
    return;

  invalidation_flags_.SetWholeSubtreeInvalid(true);
  invalidation_flags_.SetInvalidateCustomPseudo(false);
  invalidation_flags_.SetTreeBoundaryCrossing(false);
  invalidation_flags_.SetInsertionPointCrossing(false);
  invalidation_flags_.SetInvalidatesSlotted(false);
  invalidation_flags_.SetInvalidatesParts(false);
  ClearAllBackings();
}

namespace {

scoped_refptr<DescendantInvalidationSet> CreateSelfInvalidationSet() {
  auto new_set = DescendantInvalidationSet::Create();
  new_set->SetInvalidatesSelf();
  return new_set;
}

scoped_refptr<DescendantInvalidationSet> CreatePartInvalidationSet() {
  auto new_set = DescendantInvalidationSet::Create();
  new_set->SetInvalidatesParts();
  new_set->SetTreeBoundaryCrossing();
  return new_set;
}

}  // namespace

InvalidationSet* InvalidationSet::SelfInvalidationSet() {
  DEFINE_STATIC_REF(InvalidationSet, singleton_, CreateSelfInvalidationSet());
  return singleton_;
}

InvalidationSet* InvalidationSet::PartInvalidationSet() {
  DEFINE_STATIC_REF(InvalidationSet, singleton_, CreatePartInvalidationSet());
  return singleton_;
}

void InvalidationSet::ToTracedValue(TracedValue* value) const {
  value->BeginDictionary();

  value->SetString("id", DescendantInvalidationSetToIdString(*this));

  if (invalidation_flags_.WholeSubtreeInvalid())
    value->SetBoolean("allDescendantsMightBeInvalid", true);
  if (invalidation_flags_.InvalidateCustomPseudo())
    value->SetBoolean("customPseudoInvalid", true);
  if (invalidation_flags_.TreeBoundaryCrossing())
    value->SetBoolean("treeBoundaryCrossing", true);
  if (invalidation_flags_.InsertionPointCrossing())
    value->SetBoolean("insertionPointCrossing", true);
  if (invalidation_flags_.InvalidatesSlotted())
    value->SetBoolean("invalidatesSlotted", true);
  if (invalidation_flags_.InvalidatesParts())
    value->SetBoolean("invalidatesParts", true);

  if (HasIds()) {
    value->BeginArray("ids");
    for (const auto& id : Ids())
      value->PushString(id);
    value->EndArray();
  }

  if (HasClasses()) {
    value->BeginArray("classes");
    for (const auto& class_name : Classes())
      value->PushString(class_name);
    value->EndArray();
  }

  if (HasTagNames()) {
    value->BeginArray("tagNames");
    for (const auto& tag_name : TagNames())
      value->PushString(tag_name);
    value->EndArray();
  }

  if (HasAttributes()) {
    value->BeginArray("attributes");
    for (const auto& attribute : Attributes())
      value->PushString(attribute);
    value->EndArray();
  }

  value->EndDictionary();
}

#ifndef NDEBUG
void InvalidationSet::Show() const {
  auto value = std::make_unique<TracedValue>();
  value->BeginArray("InvalidationSet");
  ToTracedValue(value.get());
  value->EndArray();
  fprintf(stderr, "%s\n", value->ToString().Ascii().c_str());
}
#endif  // NDEBUG

SiblingInvalidationSet::SiblingInvalidationSet(
    scoped_refptr<DescendantInvalidationSet> descendants)
    : InvalidationSet(InvalidationType::kInvalidateSiblings),
      max_direct_adjacent_selectors_(1),
      descendant_invalidation_set_(std::move(descendants)) {}

SiblingInvalidationSet::SiblingInvalidationSet()
    : InvalidationSet(InvalidationType::kInvalidateNthSiblings),
      max_direct_adjacent_selectors_(kDirectAdjacentMax) {}

DescendantInvalidationSet& SiblingInvalidationSet::EnsureSiblingDescendants() {
  if (!sibling_descendant_invalidation_set_)
    sibling_descendant_invalidation_set_ = DescendantInvalidationSet::Create();
  return *sibling_descendant_invalidation_set_;
}

DescendantInvalidationSet& SiblingInvalidationSet::EnsureDescendants() {
  if (!descendant_invalidation_set_)
    descendant_invalidation_set_ = DescendantInvalidationSet::Create();
  return *descendant_invalidation_set_;
}

}  // namespace blink
