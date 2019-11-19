// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PSEUDO_ELEMENT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PSEUDO_ELEMENT_DATA_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class PseudoElementData final : public GarbageCollected<PseudoElementData> {
 public:
  PseudoElementData() = default;

  void SetPseudoElement(PseudoId, PseudoElement*);
  PseudoElement* GetPseudoElement(PseudoId) const;
  bool HasPseudoElements() const;
  void ClearPseudoElements();
  void Trace(Visitor* visitor) {
    visitor->Trace(generated_before_);
    visitor->Trace(generated_after_);
    visitor->Trace(generated_first_letter_);
    visitor->Trace(backdrop_);
  }

 private:
  Member<PseudoElement> generated_before_;
  Member<PseudoElement> generated_after_;
  Member<PseudoElement> generated_first_letter_;
  Member<PseudoElement> backdrop_;
  DISALLOW_COPY_AND_ASSIGN(PseudoElementData);
};

inline bool PseudoElementData::HasPseudoElements() const {
  return generated_before_ || generated_after_ || backdrop_ ||
         generated_first_letter_;
}

inline void PseudoElementData::ClearPseudoElements() {
  SetPseudoElement(kPseudoIdBefore, nullptr);
  SetPseudoElement(kPseudoIdAfter, nullptr);
  SetPseudoElement(kPseudoIdBackdrop, nullptr);
  SetPseudoElement(kPseudoIdFirstLetter, nullptr);
}

inline void PseudoElementData::SetPseudoElement(PseudoId pseudo_id,
                                                PseudoElement* element) {
  switch (pseudo_id) {
    case kPseudoIdBefore:
      if (generated_before_)
        generated_before_->Dispose();
      generated_before_ = element;
      break;
    case kPseudoIdAfter:
      if (generated_after_)
        generated_after_->Dispose();
      generated_after_ = element;
      break;
    case kPseudoIdBackdrop:
      if (backdrop_)
        backdrop_->Dispose();
      backdrop_ = element;
      break;
    case kPseudoIdFirstLetter:
      if (generated_first_letter_)
        generated_first_letter_->Dispose();
      generated_first_letter_ = element;
      break;
    default:
      NOTREACHED();
  }
}

inline PseudoElement* PseudoElementData::GetPseudoElement(
    PseudoId pseudo_id) const {
  if (kPseudoIdBefore == pseudo_id)
    return generated_before_;
  if (kPseudoIdAfter == pseudo_id)
    return generated_after_;
// Workaround for CPU bug. This avoids compiler optimizing
// this group of if conditions into switch. See http://crbug.com/855390.
#if defined(ARCH_CPU_ARMEL)
  __asm__ volatile("");
#endif
  if (kPseudoIdBackdrop == pseudo_id)
    return backdrop_;
  if (kPseudoIdFirstLetter == pseudo_id)
    return generated_first_letter_;
  return nullptr;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PSEUDO_ELEMENT_DATA_H_
