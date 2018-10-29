// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_DATA_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css/css_timing_data.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT CSSTransitionData final : public CSSTimingData {
 public:
  enum TransitionPropertyType {
    kTransitionNone,
    kTransitionKnownProperty,
    kTransitionUnknownProperty,
  };

  // FIXME: We shouldn't allow 'none' to be used alongside other properties.
  struct TransitionProperty {
    DISALLOW_NEW();
    TransitionProperty(CSSPropertyID id)
        : property_type(kTransitionKnownProperty), unresolved_property(id) {
      DCHECK_NE(id, CSSPropertyInvalid);
    }

    TransitionProperty(const AtomicString& string)
        : property_type(kTransitionUnknownProperty),
          unresolved_property(CSSPropertyInvalid),
          property_string(string) {}

    TransitionProperty(TransitionPropertyType type)
        : property_type(type), unresolved_property(CSSPropertyInvalid) {
      DCHECK_EQ(type, kTransitionNone);
    }

    bool operator==(const TransitionProperty& other) const {
      return property_type == other.property_type &&
             unresolved_property == other.unresolved_property &&
             property_string == other.property_string;
    }

    TransitionPropertyType property_type;
    CSSPropertyID unresolved_property;
    AtomicString property_string;
  };

  static std::unique_ptr<CSSTransitionData> Create() {
    return base::WrapUnique(new CSSTransitionData);
  }

  std::unique_ptr<CSSTransitionData> Clone() {
    return base::WrapUnique(new CSSTransitionData(*this));
  }

  bool TransitionsMatchForStyleRecalc(const CSSTransitionData& other) const;
  bool operator==(const CSSTransitionData& other) const {
    return TransitionsMatchForStyleRecalc(other);
  }

  Timing ConvertToTiming(size_t index) const;

  const Vector<TransitionProperty>& PropertyList() const {
    return property_list_;
  }
  Vector<TransitionProperty>& PropertyList() { return property_list_; }

  static TransitionProperty InitialProperty() {
    return TransitionProperty(CSSPropertyAll);
  }

 private:
  CSSTransitionData();
  explicit CSSTransitionData(const CSSTransitionData&);

  Vector<TransitionProperty> property_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TRANSITION_DATA_H_
