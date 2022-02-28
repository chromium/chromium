// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRANSITION_PSEUDO_ELEMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRANSITION_PSEUDO_ELEMENT_DATA_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
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
    visitor->Trace(transition_outgoing_image_);
    visitor->Trace(transition_incoming_image_);
    visitor->Trace(transition_image_wrapper_);
    visitor->Trace(transition_containers_);
  }

 private:
  Member<PseudoElement> transition_;
  Member<PseudoElement> transition_outgoing_image_;
  Member<PseudoElement> transition_incoming_image_;
  Member<PseudoElement> transition_image_wrapper_;
  HeapHashMap<AtomicString, Member<PseudoElement>> transition_containers_;
};

inline bool TransitionPseudoElementData::HasPseudoElements() const {
  return transition_ || transition_outgoing_image_ ||
         transition_incoming_image_ || transition_image_wrapper_ ||
         !transition_containers_.IsEmpty();
}

inline void TransitionPseudoElementData::ClearPseudoElements() {
  SetPseudoElement(kPseudoIdPageTransition, nullptr);
  SetPseudoElement(kPseudoIdPageTransitionImageWrapper, nullptr,
                   transition_image_wrapper_
                       ? transition_image_wrapper_->document_transition_tag()
                       : g_null_atom);
  SetPseudoElement(kPseudoIdPageTransitionOutgoingImage, nullptr,
                   transition_outgoing_image_
                       ? transition_outgoing_image_->document_transition_tag()
                       : g_null_atom);
  SetPseudoElement(kPseudoIdPageTransitionIncomingImage, nullptr,
                   transition_incoming_image_
                       ? transition_incoming_image_->document_transition_tag()
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
    case kPseudoIdPageTransition:
      previous_element = transition_;
      transition_ = element;
      break;
    case kPseudoIdPageTransitionImageWrapper:
      DCHECK(!element ||
             element->document_transition_tag() == document_transition_tag);
      DCHECK(!transition_image_wrapper_ ||
             transition_image_wrapper_->document_transition_tag() ==
                 document_transition_tag);

      previous_element = transition_image_wrapper_;
      transition_image_wrapper_ = element;
      break;
    case kPseudoIdPageTransitionOutgoingImage:
      DCHECK(!element ||
             element->document_transition_tag() == document_transition_tag);
      DCHECK(!transition_outgoing_image_ ||
             transition_outgoing_image_->document_transition_tag() ==
                 document_transition_tag);

      previous_element = transition_outgoing_image_;
      transition_outgoing_image_ = element;
      break;
    case kPseudoIdPageTransitionIncomingImage:
      DCHECK(!element ||
             element->document_transition_tag() == document_transition_tag);
      DCHECK(!transition_incoming_image_ ||
             transition_incoming_image_->document_transition_tag() ==
                 document_transition_tag);

      previous_element = transition_incoming_image_;
      transition_incoming_image_ = element;
      break;
    case kPseudoIdPageTransitionContainer: {
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
  if (kPseudoIdPageTransition == pseudo_id)
    return transition_;
  if (kPseudoIdPageTransitionImageWrapper == pseudo_id) {
    DCHECK(document_transition_tag);
    if (transition_image_wrapper_ &&
        transition_image_wrapper_->document_transition_tag() ==
            document_transition_tag) {
      return transition_image_wrapper_;
    }
    return nullptr;
  }
  if (kPseudoIdPageTransitionOutgoingImage == pseudo_id) {
    DCHECK(document_transition_tag);
    if (transition_outgoing_image_ &&
        transition_outgoing_image_->document_transition_tag() ==
            document_transition_tag) {
      return transition_outgoing_image_;
    }
    return nullptr;
  }
  if (kPseudoIdPageTransitionIncomingImage == pseudo_id) {
    DCHECK(document_transition_tag);
    if (transition_incoming_image_ &&
        transition_incoming_image_->document_transition_tag() ==
            document_transition_tag) {
      return transition_incoming_image_;
    }
    return nullptr;
  }
  if (kPseudoIdPageTransitionContainer == pseudo_id) {
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
  if (transition_image_wrapper_)
    result->push_back(transition_image_wrapper_);
  if (transition_outgoing_image_)
    result->push_back(transition_outgoing_image_);
  if (transition_incoming_image_)
    result->push_back(transition_incoming_image_);
  for (const auto& entry : transition_containers_)
    result->push_back(entry.value);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRANSITION_PSEUDO_ELEMENT_DATA_H_
