// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PSEUDO_ELEMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PSEUDO_ELEMENT_DATA_H_

#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/transition_pseudo_element_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class PseudoElementData final : public GarbageCollected<PseudoElementData>,
                                public ElementRareDataField {
 public:
  PseudoElementData() = default;
  PseudoElementData(const PseudoElementData&) = delete;
  PseudoElementData& operator=(const PseudoElementData&) = delete;

  void SetPseudoElement(PseudoId,
                        PseudoElement*,
                        const AtomicString& view_transition_name = g_null_atom);
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& view_transition_name = g_null_atom) const;

  using PseudoElementVector = HeapVector<Member<PseudoElement>, 2>;
  PseudoElementVector GetPseudoElements() const;

  bool HasPseudoElements() const;
  void ClearPseudoElements();
  void Trace(Visitor* visitor) const override {
    visitor->Trace(generated_before_);
    visitor->Trace(generated_after_);
    visitor->Trace(generated_marker_);
    visitor->Trace(generated_first_letter_);
    visitor->Trace(backdrop_);
    visitor->Trace(transition_data_);
    ElementRareDataField::Trace(visitor);
  }

 private:
  Member<PseudoElement> generated_before_;
  Member<PseudoElement> generated_after_;
  Member<PseudoElement> generated_marker_;
  Member<PseudoElement> generated_first_letter_;
  Member<PseudoElement> backdrop_;

  Member<TransitionPseudoElementData> transition_data_;
};

inline bool PseudoElementData::HasPseudoElements() const {
  return generated_before_ || generated_after_ || generated_marker_ ||
         backdrop_ || generated_first_letter_ || transition_data_;
}

inline void PseudoElementData::ClearPseudoElements() {
  SetPseudoElement(kPseudoIdBefore, nullptr);
  SetPseudoElement(kPseudoIdAfter, nullptr);
  SetPseudoElement(kPseudoIdMarker, nullptr);
  SetPseudoElement(kPseudoIdBackdrop, nullptr);
  SetPseudoElement(kPseudoIdFirstLetter, nullptr);
  if (transition_data_) {
    transition_data_->ClearPseudoElements();
    transition_data_ = nullptr;
  }
}

inline void PseudoElementData::SetPseudoElement(
    PseudoId pseudo_id,
    PseudoElement* element,
    const AtomicString& view_transition_name) {
  PseudoElement* previous_element = nullptr;
  switch (pseudo_id) {
    case kPseudoIdBefore:
      previous_element = generated_before_;
      generated_before_ = element;
      break;
    case kPseudoIdAfter:
      previous_element = generated_after_;
      generated_after_ = element;
      break;
    case kPseudoIdMarker:
      previous_element = generated_marker_;
      generated_marker_ = element;
      break;
    case kPseudoIdBackdrop:
      previous_element = backdrop_;
      backdrop_ = element;
      break;
    case kPseudoIdFirstLetter:
      previous_element = generated_first_letter_;
      generated_first_letter_ = element;
      break;
    case kPseudoIdViewTransition:
    case kPseudoIdViewTransitionGroup:
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionNew:
    case kPseudoIdViewTransitionOld:
      if (element && !transition_data_)
        transition_data_ = MakeGarbageCollected<TransitionPseudoElementData>();
      if (transition_data_) {
        transition_data_->SetPseudoElement(pseudo_id, element,
                                           view_transition_name);
        if (!transition_data_->HasPseudoElements())
          transition_data_ = nullptr;
      }
      break;
    default:
      NOTREACHED();
  }

  if (previous_element)
    previous_element->Dispose();
}

inline PseudoElement* PseudoElementData::GetPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) const {
  if (kPseudoIdBefore == pseudo_id)
    return generated_before_;
  if (kPseudoIdAfter == pseudo_id)
    return generated_after_;
  if (kPseudoIdMarker == pseudo_id)
    return generated_marker_;
// Workaround for CPU bug. This avoids compiler optimizing
// this group of if conditions into switch. See http://crbug.com/855390.
#if defined(ARCH_CPU_ARMEL)
  __asm__ volatile("");
#endif
  if (kPseudoIdBackdrop == pseudo_id)
    return backdrop_;
  if (kPseudoIdFirstLetter == pseudo_id)
    return generated_first_letter_;
  if (IsTransitionPseudoElement(pseudo_id)) {
    return transition_data_ ? transition_data_->GetPseudoElement(
                                  pseudo_id, view_transition_name)
                            : nullptr;
  }
  return nullptr;
}

inline PseudoElementData::PseudoElementVector
PseudoElementData::GetPseudoElements() const {
  PseudoElementData::PseudoElementVector result;
  if (generated_before_)
    result.push_back(generated_before_);
  if (generated_after_)
    result.push_back(generated_after_);
  if (generated_marker_)
    result.push_back(generated_marker_);
  if (generated_first_letter_)
    result.push_back(generated_first_letter_);
  if (backdrop_)
    result.push_back(backdrop_);
  if (transition_data_)
    transition_data_->AddPseudoElements(&result);
  return result;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PSEUDO_ELEMENT_DATA_H_
