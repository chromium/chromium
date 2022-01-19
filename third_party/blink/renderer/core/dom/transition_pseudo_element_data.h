// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRANSITION_PSEUDO_ELEMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRANSITION_PSEUDO_ELEMENT_DATA_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class TransitionPseudoElementData final
    : public GarbageCollected<TransitionPseudoElementData> {
 public:
  TransitionPseudoElementData() = default;
  TransitionPseudoElementData(const TransitionPseudoElementData&) = delete;
  TransitionPseudoElementData& operator=(const TransitionPseudoElementData&) =
      delete;

  void SetPseudoElement(
      PseudoId,
      PseudoElement*,
      const AtomicString& document_transition_tag = g_null_atom);
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& document_transition_tag = g_null_atom) const;

  void AddPseudoElements(HeapVector<Member<PseudoElement>, 2>* result) const;

  bool HasPseudoElements() const;
  void ClearPseudoElements();
  void Trace(Visitor* visitor) const {
    visitor->Trace(transition_);
    visitor->Trace(transition_old_content_);
    visitor->Trace(transition_new_content_);
    visitor->Trace(transition_containers_);
  }

 private:
  Member<PseudoElement> transition_;
  Member<PseudoElement> transition_old_content_;
  Member<PseudoElement> transition_new_content_;
  HeapHashMap<AtomicString, Member<PseudoElement>> transition_containers_;
};

inline bool TransitionPseudoElementData::HasPseudoElements() const {
  return transition_ || transition_old_content_ || transition_new_content_ ||
         !transition_containers_.IsEmpty();
}

inline void TransitionPseudoElementData::ClearPseudoElements() {
  SetPseudoElement(kPseudoIdTransition, nullptr);
  SetPseudoElement(kPseudoIdTransitionOldContent, nullptr,
                   transition_old_content_
                       ? transition_old_content_->document_transition_tag()
                       : g_null_atom);
  SetPseudoElement(kPseudoIdTransitionNewContent, nullptr,
                   transition_new_content_
                       ? transition_new_content_->document_transition_tag()
                       : g_null_atom);

  for (auto& entry : transition_containers_)
    entry.value->Dispose();
  transition_containers_.clear();
}

inline void TransitionPseudoElementData::SetPseudoElement(
    PseudoId pseudo_id,
    PseudoElement* element,
    const AtomicString& document_transition_tag) {
  PseudoElement* previous_element = nullptr;
  switch (pseudo_id) {
    case kPseudoIdTransition:
      previous_element = transition_;
      transition_ = element;
      break;
    case kPseudoIdTransitionOldContent:
      DCHECK(!element ||
             element->document_transition_tag() == document_transition_tag);
      DCHECK(!transition_old_content_ ||
             transition_old_content_->document_transition_tag() ==
                 document_transition_tag);

      previous_element = transition_old_content_;
      transition_old_content_ = element;
      break;
    case kPseudoIdTransitionNewContent:
      DCHECK(!element ||
             element->document_transition_tag() == document_transition_tag);
      DCHECK(!transition_new_content_ ||
             transition_new_content_->document_transition_tag() ==
                 document_transition_tag);

      previous_element = transition_new_content_;
      transition_new_content_ = element;
      break;
    case kPseudoIdTransitionContainer: {
      DCHECK(document_transition_tag);

      auto it = transition_containers_.find(document_transition_tag);
      previous_element =
          it == transition_containers_.end() ? nullptr : it->value;
      if (element) {
        DCHECK_EQ(element->document_transition_tag(), document_transition_tag);
        transition_containers_.Set(element->document_transition_tag(), element);
      } else {
        transition_containers_.erase(it);
      }
      break;
    }
    default:
      NOTREACHED();
  }

  if (previous_element)
    previous_element->Dispose();
}

inline PseudoElement* TransitionPseudoElementData::GetPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag) const {
  if (kPseudoIdTransition == pseudo_id)
    return transition_;
  if (kPseudoIdTransitionOldContent == pseudo_id) {
    DCHECK(document_transition_tag);
    if (transition_old_content_ &&
        transition_old_content_->document_transition_tag() ==
            document_transition_tag) {
      return transition_old_content_;
    }
    return nullptr;
  }
  if (kPseudoIdTransitionNewContent == pseudo_id) {
    DCHECK(document_transition_tag);
    if (transition_new_content_ &&
        transition_new_content_->document_transition_tag() ==
            document_transition_tag) {
      return transition_new_content_;
    }
    return nullptr;
  }
  if (kPseudoIdTransitionContainer == pseudo_id) {
    DCHECK(document_transition_tag);
    auto it = transition_containers_.find(document_transition_tag);
    return it == transition_containers_.end() ? nullptr : it->value;
  }
  return nullptr;
}

inline void TransitionPseudoElementData::AddPseudoElements(
    HeapVector<Member<PseudoElement>, 2>* result) const {
  if (transition_)
    result->push_back(transition_);
  if (transition_old_content_)
    result->push_back(transition_old_content_);
  if (transition_new_content_)
    result->push_back(transition_new_content_);
  for (const auto& entry : transition_containers_)
    result->push_back(entry.value);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRANSITION_PSEUDO_ELEMENT_DATA_H_
