// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SETLIKE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SETLIKE_H_

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// Utility class to simplify implementation of various `setlike` classes.
// The consumer of the class needs only to implement the |elements()| method -
// everything else should be provided by this class. For examples, see
// `XRPlaneSet` and `XRAnchorSet`.
template <typename ElementType>
class XRSetlike : public SetlikeIterable<Member<ElementType>> {
 public:
  unsigned size() const { return elements().size(); }

  // Returns true if passed in |element| is a member of the XRSetlike.
  bool hasForBinding(ScriptState* script_state,
                     ElementType* element,
                     ExceptionState& exception_state) const {
    DCHECK(element);
    auto all_elements = elements();
    return all_elements.find(element) != all_elements.end();
  }

 protected:
  virtual const HeapHashSet<Member<ElementType>>& elements() const = 0;

 private:
  class IterationSource final
      : public SetlikeIterable<Member<ElementType>>::IterationSource {
   public:
    explicit IterationSource(const HeapHashSet<Member<ElementType>>& elements)
        : index_(0) {
      elements_.ReserveInitialCapacity(elements.size());
      for (auto element : elements) {
        elements_.push_back(element);
      }
    }

    bool Next(ScriptState* script_state,
              Member<ElementType>& key,
              Member<ElementType>& value,
              ExceptionState& exception_state) override {
      if (index_ >= elements_.size()) {
        return false;
      }

      key = value = elements_[index_];
      ++index_;

      return true;
    }

    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(elements_);
      SetlikeIterable<Member<ElementType>>::IterationSource::Trace(visitor);
    }

   private:
    HeapVector<Member<ElementType>> elements_;

    unsigned index_;
  };

  // Starts iteration over XRSetlike.
  // Needed for SetlikeIterable to work properly.
  XRSetlike::IterationSource* StartIteration(
      ScriptState* script_state,
      ExceptionState& exception_state) override {
    return MakeGarbageCollected<XRSetlike::IterationSource>(elements());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SETLIKE_H_
